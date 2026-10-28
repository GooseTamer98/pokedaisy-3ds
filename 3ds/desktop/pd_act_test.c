// The game actions (pd_action.c) run for real: a decomp ROM (= retail) booted
// in a host libmgba, a RAM fixture loaded over its running game (the field the
// fixture was captured on - its logic runs, its VRAM is the title screen's),
// then each action through pd_mgba.c's calls, checked in RAM by the snapshot
// reader and the game's own functions.
//
//   pd_act_test <rom.gba> <fixture dir> [battle | tables]
//
// `make act-test` (needs `make decomps` and a host libmgba: see the Makefile).
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/gba/core.h>
#include <mgba-util/vfs.h>

#include "pd_action.h"
#include "pd_bctl.h"
#include "pd_game.h"
#include "pd_mgba.h"
#include "pd_snapshot.h"
#include "pd_tables.h"

static void quiet(struct mLogger* l, int c, enum mLogLevel lv, const char* f, va_list a) {
    (void) l, (void) c, (void) lv, (void) f, (void) a;
}
static struct mLogger silent = { .log = quiet };

static struct mCore* core;
static struct pd_game game;
static struct pd_game_io io;
static int failures;

static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static struct pd_act* stepping; // the host's step runs after every frame, as on the 3DS
static bool rd(void* ctx, uint32_t addr, void* out, size_t len);

static void frames(int n) {
    for (int i = 0; i < n; i++) {
        core->runFrame(core);
        pd_mgba_frame_done(core);
        if (stepping && stepping->kind == PD_REQ_NONE) pd_act_step(stepping, &game, &io, false);
    }
}

static bool rd(void* ctx, uint32_t addr, void* out, size_t len) {
    return io.read(ctx, addr, out, len);
}

static void snap(struct pd_snapshot* s) {
    pd_snapshot_read(s, &game, rd, core);
}

static void load_ram(const char* dir, const char* name, uint32_t base) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "no %s\n", path);
        exit(2);
    }
    static uint8_t buf[0x40000];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    io.write(core, base, buf, n);
}

// Runs a request to its answer, a frame at a time like the host.
static void run(struct pd_act* a, enum pd_act_kind kind) {
    pd_act_request(a, kind);
    for (int i = 0; i < 200; i++) {
        struct pd_snapshot s;
        snap(&s);
        if (pd_act_step(a, &game, &io, s.inBattle)) return;
        frames(1);
    }
    a->result = PD_ACT_NONE;
}

static int count_of(const struct pd_snapshot* s, int item) {
    for (int p = 0; p < PD_POCKET_COUNT; p++) {
        for (int i = 0; i < s->bag[p].count; i++) {
            if (s->bag[p].items[i].id == item) return s->bag[p].items[i].quantity;
        }
    }
    return 0;
}

static uint32_t call(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2) {
    const uint32_t args[4] = { a0, a1, a2, 0 };
    uint32_t r0 = 0;
    if (!io.call(core, fn, args, &r0)) printf("     (call %08x refused)\n", fn);
    return r0;
}

// The game's functions the test itself calls (each build's map): the field-lock
// check, ScriptContext_SetupScript, SetMonData, AddBagItem.
struct test_fns {
    uint32_t lockFn, setupScript, setMonData, addBagItem;
};

static struct test_fns test_fns(void) {
    bool rev0 = game.revision == 0;
    if (game.kind == PD_GAME_EMERALD && !pd_game_is_rs(&game)) return (struct test_fns) { 0x08098E6C, 0x08098EF8, 0x0806ACAC, 0x080D6928 };
    if (!strcmp(game.code, "AXVE")) {
        return rev0 ? (struct test_fns) { 0x08065548, 0x080655B8, 0x0803D1FC, 0x080A9424 }
                    : (struct test_fns) { 0x08065568, 0x080655D8, 0x0803D1FC, 0x080A9444 };
    }
    if (!strcmp(game.code, "AXPE")) {
        return rev0 ? (struct test_fns) { 0x0806554C, 0x080655BC, 0x0803D1FC, 0x080A9424 }
                    : (struct test_fns) { 0x0806556C, 0x080655DC, 0x0803D1FC, 0x080A9444 };
    }
    if (!strcmp(game.code, "BPGE") && rev0) return (struct test_fns) { 0x08069958, 0x08069AE4, 0x0804037C, 0x0809A058 };
    if (!strcmp(game.code, "BPGE")) return (struct test_fns) { 0x0806996C, 0x08069AF8, 0x08040390, 0x0809A06C };
    return (struct test_fns) { 0x0806996C, 0x08069AF8, 0x08040390, 0x0809A098 };
}

// FireRed's quest-log recap (firered_vanilla was captured mid-way): its state
// cleared (sLockFieldControls, gQuestLogState) so the field is the player's.
static void clear_quest_log(void) {
    if (game.kind != PD_GAME_FIRERED) return;
    uint8_t zero = 0;
    io.write(core, 0x03000F9C, &zero, 1);
    io.write(core, 0x0203ADFA, &zero, 1);
    frames(2);
}

// --- BATTLE: the companion's controls on the game's own battle menu ---------

static struct pd_bctl bc;

static void battle_frames(int n) {
    for (int i = 0; i < n; i++) {
        core->setKeys(core, pd_bctl_frame(&bc, &game, &io, 0));
        core->runFrame(core);
        pd_mgba_frame_done(core);
    }
}

// Frames until the menu waits (or limit), pressing A now and then through
// messages that wait for it; the state then.
static enum pd_bin wait_menu(int limit, bool mash) {
    int battler;
    for (int i = 0; i < limit; i++) {
        enum pd_bin st = pd_battle_input(&game, io.read, core, &battler);
        if (!pd_bctl_busy(&bc) && (st == PD_BIN_ACTION || st == PD_BIN_NONE || st == PD_BIN_PARTY)) return st;
        if (mash && !pd_bctl_busy(&bc) && i % 40 == 39) {
            core->setKeys(core, 1);
            core->runFrame(core);
            pd_mgba_frame_done(core);
        }
        battle_frames(1);
    }
    return pd_battle_input(&game, io.read, core, &battler);
}

// setwildbattle CHANSEY 60 (no item); dowildbattle; end - through the game's
// script engine, from the field; true once the action menu waits.
static bool start_battle(uint32_t setupScript) {
    const uint8_t script[] = { 0xB6, 113, 0, 60, 0, 0, 0xB7, 0x02 };
    io.write(core, 0x0203FFF4, script, sizeof(script));
    call(setupScript, 0x0203FFF4, 0, 0);
    int b0;
    for (int i = 0; i < 1200 && pd_battle_input(&game, io.read, core, &b0) == PD_BIN_NONE; i++) battle_frames(1);
    check(pd_battle_input(&game, io.read, core, &b0) != PD_BIN_NONE, "the battle started");
    return wait_menu(4000, true) == PD_BIN_ACTION;
}

static void battle_test(uint32_t setupScript) {
    check(pd_battle_input_known(&game), "battle input: the handlers are known");
    check(start_battle(setupScript), "a wild battle: the action menu waits");
    enum pd_bin st;
    struct pd_snapshot s;
    snap(&s);
    int pp0 = s.battlers[PD_POS_PLAYER_LEFT].pp[0], lead = s.battlers[PD_POS_PLAYER_LEFT].species;
    printf("     %d vs %d, move 0 PP %d\n", lead, s.battlers[PD_POS_OPPONENT_LEFT].species, pp0);

    // BAG, then back.
    pd_bctl_action(&bc, 1);
    int battler;
    for (int i = 0; i < 200 && pd_battle_input(&game, io.read, core, &battler) != PD_BIN_BAG; i++) battle_frames(1);
    check(pd_battle_input(&game, io.read, core, &battler) == PD_BIN_BAG, "BAG opens the game's bag");
    battle_frames(60);
    pd_bctl_back(&bc);
    battle_frames(2);
    for (int i = 0; i < 300 && pd_battle_input(&game, io.read, core, &battler) != PD_BIN_ACTION; i++) {
        if (i % 60 == 59) pd_bctl_back(&bc);
        battle_frames(1);
    }
    check(pd_battle_input(&game, io.read, core, &battler) == PD_BIN_ACTION, "B: back to the action menu");

    // FIGHT + move 0 in one tap: one PP gone.
    pd_bctl_move(&bc, 0);
    int taken = 0;
    for (int i = 0; i < 600; i++) {
        battle_frames(1);
        snap(&s);
        if (s.battlers[PD_POS_PLAYER_LEFT].pp[0] == pp0 - 1) taken = 1;
        if (taken) break;
    }
    check(taken, "FIGHT + the move: its PP spent");
    st = wait_menu(3000, true);
    if (st == PD_BIN_NONE) {
        // The move won the battle (ruby_rev1's RAYQUAZA Lv100): another one.
        printf("     the foe fainted: a new battle\n");
        for (int i = 0; i < 300; i++) battle_frames(1);
        st = start_battle(setupScript) ? PD_BIN_ACTION : PD_BIN_NONE;
    }
    check(st == PD_BIN_ACTION, "the next turn's menu");

    if (!game.cfg->partyMenu) {
        // No switch of ours (Ruby / Sapphire): POKéMON opens the game's party screen, B closes it.
        pd_bctl_action(&bc, 2);
        for (int i = 0; i < 300 && pd_battle_input(&game, io.read, core, &battler) != PD_BIN_PARTY; i++) battle_frames(1);
        check(pd_battle_input(&game, io.read, core, &battler) == PD_BIN_PARTY, "POKéMON opens the game's party screen");
        battle_frames(60);
        pd_bctl_back(&bc);
        for (int i = 0; i < 300 && pd_battle_input(&game, io.read, core, &battler) != PD_BIN_ACTION; i++) {
            if (i % 60 == 59) pd_bctl_back(&bc);
            battle_frames(1);
        }
        check(pd_battle_input(&game, io.read, core, &battler) == PD_BIN_ACTION, "B: back to the action menu");
    } else {
    // POKéMON: party slot 2 sent in.
    snap(&s);
    int want = -1;
    for (int i = 1; i < s.partyCount && want < 0; i++) {
        if (s.party[i].hp > 0 && s.party[i].species != PD_SPECIES_EGG && s.party[i].species != lead) want = i;
    }
    pd_bctl_switch(&bc, s.party[want].personality);
    int sent = 0;
    for (int i = 0; i < 1500 && !sent; i++) {
        battle_frames(1);
        struct pd_snapshot t;
        snap(&t);
        sent = t.battlers[PD_POS_PLAYER_LEFT].species == s.party[want].species;
    }
    printf("     switch to slot %d (%d): %s\n", want, s.party[want].species, sent ? "in" : "not in");
    check(sent, "POKéMON: the picked one is out");
    st = wait_menu(3000, true);
    check(st == PD_BIN_ACTION, "the next turn's menu");
    }

    // RUN until it works (a wild battle can refuse): the battle ends.
    bool ended = false;
    for (int tries = 0; tries < 5 && !ended; tries++) {
        pd_bctl_action(&bc, 3);
        for (int i = 0; i < 900 && !ended; i++) {
            battle_frames(1);
            ended = pd_battle_input(&game, io.read, core, &battler) == PD_BIN_NONE;
        }
        if (!ended) wait_menu(2000, true);
    }
    check(ended, "RUN: the battle ends");
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <rom.gba> <fixture dir>\n", argv[0]);
        return 2;
    }
    mLogSetDefaultLogger(&silent);
    core = GBACoreCreate();
    core->init(core);
    mCoreInitConfig(core, NULL);
    struct VFile* vf = VFileOpen(argv[1], O_RDONLY);
    if (!vf || !core->loadROM(core, vf)) {
        fprintf(stderr, "cannot load %s\n", argv[1]);
        return 2;
    }
    static color_t video[240 * 160];
    core->setVideoBuffer(core, video, 240);
    core->reset(core);
    pd_mgba_io(&io, core);
    // The companion reads the ROM file's own bytes (the host's game.rom).
    static uint8_t rom[32 << 20];
    FILE* rf = fopen(argv[1], "rb");
    size_t romSize = rf ? fread(rom, 1, sizeof(rom), rf) : 0;
    if (rf) fclose(rf);
    pd_game_detect(&game, rom, romSize);
    game.rom = rom;
    game.romSize = romSize;
    pd_game_identify(&game);
    printf("%s rev %d: %s\n", game.code, game.revision, game.title);
    frames(600);
    load_ram(argv[2], "ewram.bin", 0x02000000);
    load_ram(argv[2], "iwram.bin", 0x03000000);
    pd_mgba_reset();
    frames(60);

    const struct test_fns fns = test_fns();
    if (argc > 3 && !strcmp(argv[3], "battle")) {
        if (call(fns.lockFn, 0, 0, 0)) clear_quest_log();
        battle_test(fns.setupScript);
        printf("%d failure(s)\n", failures);
        return failures ? 1 : 0;
    }
    check(pd_act_supported(&game), "the ROM's code is where the tables say");
    check(pd_act_can_pc(&game), "PC code found");
    check(pd_act_can_fly(&game), "FLY code found");
    check(pd_battle_input_known(&game), "battle input: the handlers are known");
    if (argc > 3 && !strcmp(argv[3], "tables")) {
        // A ROM with no fixture of its own (a fixture's RAM holds another build's
        // code pointers): only that its tables match its code.
        printf("%d failure(s)\n", failures);
        return failures ? 1 : 0;
    }
    const struct pd_config* cfg = game.cfg;
    uint32_t setMonData = fns.setMonData;
    uint32_t scratch = 0x0203FFF0;

    struct pd_act a;
    memset(&a, 0, sizeof(a));
    stepping = &a;
    uint32_t lockFn = fns.lockFn;
    if (call(lockFn, 0, 0, 0) && game.kind == PD_GAME_FIRERED) {
        // firered_vanilla was captured with the quest log's recap still on
        // (controls locked): the actions must say NOT_NOW. Then the recap's
        // state cleared (sLockFieldControls, gQuestLogState) for the rest.
        a.item = 13;
        run(&a, PD_REQ_USE);
        check(a.result == PD_ACT_NOT_NOW, "controls locked: NOT NOW");
        clear_quest_log();
    }
    check(call(lockFn, 0, 0, 0) == 0, "the field is free");
    struct pd_snapshot s;
    snap(&s);
    printf("     party %d, first %s Lv%d %d/%d\n", s.partyCount, s.party[0].nickname, s.party[0].level, s.party[0].hp,
           s.party[0].maxHp);

    // USE a Potion (13) on a hurt Pokémon: HP down to 1 through the game's SetMonData first.
    uint8_t one[2] = { 1, 0 };
    io.write(core, scratch, one, 2);
    call(setMonData, cfg->playerParty, 57, scratch);
    snap(&s);
    check(s.party[0].hp == 1, "the first Pokémon hurt to 1 HP");
    int potions = count_of(&s, 13);
    if (!potions) {
        // None in the bag: one through the game's AddBagItem.
        call(fns.addBagItem, 13, 1, 0);
        snap(&s);
        potions = count_of(&s, 13);
    }
    check(pd_act_can_use(&game, 13) && pd_act_picks_mon(&game, 13), "a POTION is USE on a Pokémon");
    a.item = 13;
    run(&a, PD_REQ_USE);
    check(a.result == PD_ACT_USED && a.targetCount == s.partyCount, "USE: the party pick");
    check(a.targets[0].verdict == PD_SLOT_OK, "the hurt one can take it");
    bool fullOne = false;
    for (int i = 1; i < a.targetCount; i++) {
        if (s.party[i].hp == s.party[i].maxHp && s.party[i].hp) fullOne |= a.targets[i].verdict == PD_SLOT_NO_EFFECT;
    }
    check(fullOne || s.partyCount < 2, "a healthy one: no effect");
    snap(&s);
    check(s.party[0].hp == 1, "the dry run left the Pokémon as it was");
    a.slot = 0;
    a.personality = a.targets[0].personality;
    run(&a, PD_REQ_USE_ON);
    snap(&s);
    check(a.result == PD_ACT_USED, "USE on the Pokémon");
    check(s.party[0].hp == (s.party[0].maxHp < 21 ? s.party[0].maxHp : 21), "it healed 20 HP");
    check(count_of(&s, 13) == potions - 1, "one POTION left the bag");

    // TAKE what someone holds, GIVE it back.
    int holder = -1;
    for (int i = 0; i < s.partyCount && holder < 0; i++) {
        if (s.party[i].heldItem) holder = i;
    }
    if (holder < 0) {
        // Nobody holds anything: GIVE a POTION first.
        a.item = 13;
        run(&a, PD_REQ_GIVE_TARGETS);
        a.slot = 0;
        a.personality = a.targets[0].personality;
        run(&a, PD_REQ_GIVE_TO);
        check(a.result == PD_ACT_GIVEN, "GIVE a POTION");
        snap(&s);
        holder = 0;
    }
    int held = s.party[holder].heldItem, before = count_of(&s, held);
    printf("     %s holds item %d (%s), %d in the bag\n", s.party[holder].nickname, held, pd_item_name(&game, held), before);
    a.slot = holder;
    a.personality = s.party[holder].personality;
    run(&a, PD_REQ_TAKE);
    snap(&s);
    check(a.result == PD_ACT_TAKEN && a.swapped == held, "TAKE");
    check(s.party[holder].heldItem == 0 && count_of(&s, held) == before + 1, "it's in the bag now");
    a.item = held;
    run(&a, PD_REQ_GIVE_TARGETS);
    check(a.result == PD_ACT_USED && a.targets[holder].verdict == PD_SLOT_OK, "GIVE: the party pick");
    run(&a, PD_REQ_GIVE_TO);
    snap(&s);
    check(a.result == PD_ACT_GIVEN && s.party[holder].heldItem == held && count_of(&s, held) == before, "GIVE it back");

    // A Repel: through the game's VarGet / VarSet, one out of the bag.
    if (!count_of(&s, 86)) {
        call(fns.addBagItem, 86, 2, 0);
        snap(&s);
    }
    int repels = count_of(&s, 86);
    a.item = 86;
    run(&a, PD_REQ_USE);
    snap(&s);
    check(a.result == PD_ACT_USED && a.steps == 100 && count_of(&s, 86) == repels - 1, "USE a REPEL: 100 steps");
    run(&a, PD_REQ_USE);
    check(a.result == PD_ACT_STILL_ACTIVE, "a second one: still working");

    // FLY: the towns, then the flight; the game's own warp lands in the town.
    run(&a, PD_REQ_FLY_TARGETS);
    printf("     FLY: result %d, %d towns, flyer slot %d\n", a.result, a.flyCount, a.flyerSlot);
    if (a.result == PD_ACT_NO_FLYER) {
        // Teach the first Pokémon FLY (move slot 4) to try it.
        uint8_t fly[2] = { 19, 0 };
        io.write(core, scratch, fly, 2);
        call(setMonData, cfg->playerParty, 16, scratch);
        run(&a, PD_REQ_FLY_TARGETS);
        printf("     FLY: result %d, %d towns, flyer slot %d\n", a.result, a.flyCount, a.flyerSlot);
    }
    if (a.result == PD_ACT_CANT_FLY_HERE) {
        // The fixture stands indoors (ruby_rev1: a Slateport house): the game's
        // own map check says no. Then the map called a city's, to fly anyway.
        snap(&s);
        check(s.mapType == 8, "indoors: can't FLY from here");
        uint8_t city = 2;
        io.write(core, cfg->mapHeader + 0x17, &city, 1);
        run(&a, PD_REQ_FLY_TARGETS);
        printf("     FLY: result %d, %d towns, flyer slot %d\n", a.result, a.flyCount, a.flyerSlot);
    }
    check(a.result == PD_ACT_USED && a.flyCount > 0, "FLY's towns");
    if (a.flyCount > 0) {
        snap(&s);
        int from = s.mapsec;
        int to = a.fly[0] == from && a.flyCount > 1 ? a.fly[1] : a.fly[0];
        a.mapsec = to;
        run(&a, PD_REQ_FLY);
        check(a.result == PD_ACT_FLYING, "FLY started");
        frames(40);
        snap(&s);
        printf("     mapsec %d -> %d (asked %d)\n", from, s.mapsec, to);
        frames(600);
        snap(&s);
        check(s.mapsec == to, "landed in the town");
        uint8_t w[4];
        io.read(core, cfg->gMain + 4, w, 4);
        printf("     callback2 %02x%02x%02x%02x\n", w[3], w[2], w[1], w[0]);
    }

    // PC: the game's script engine opens it; A then shows the storage menu.
    run(&a, PD_REQ_PC);
    check(a.result == PD_ACT_USED, "PC anywhere");
    uint8_t w[4];
    io.read(core, cfg->gMain + 4, w, 4);
    uint32_t cb2 = (uint32_t) w[0] | w[1] << 8 | w[2] << 16 | (uint32_t) w[3] << 24;
    uint32_t locked;
    frames(30);
    locked = call(lockFn, 0, 0, 0);
    check(locked != 0, "PC: controls locked by its script");
    // The menu's third choice, MOVE POKéMON (WITHDRAW refuses a full party).
    frames(60);
    for (int i = 0; i < 2; i++) {
        core->setKeys(core, 0x80); // DOWN
        frames(4);
        core->setKeys(core, 0);
        frames(20);
    }
    uint32_t after = cb2;
    for (int i = 0; i < 12 && after == cb2; i++) {
        core->setKeys(core, 1); // A
        frames(4);
        core->setKeys(core, 0);
        frames(40);
        io.read(core, cfg->gMain + 4, w, 4);
        after = (uint32_t) w[0] | w[1] << 8 | w[2] << 16 | (uint32_t) w[3] << 24;
    }
    printf("     callback2 %08x -> %08x\n", cb2, after);
    check(after != cb2, "PC: A opens the storage system's own screen");

    printf("%d failure(s)\n", failures);
    core->deinit(core);
    return failures ? 1 : 0;
}
