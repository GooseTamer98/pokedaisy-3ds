// The game actions (pd_action.c) run for real: a decomp ROM (= retail) booted
// in a host libmgba, a RAM fixture loaded over its running game (the field the
// fixture was captured on - its logic runs, its VRAM is the title screen's),
// then each action through pd_mgba.c's calls, checked in RAM by the snapshot
// reader and the game's own functions.
//
//   pd_act_test <rom.gba> <fixture dir>
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
#include "pd_game.h"
#include "pd_mgba.h"
#include "pd_snapshot.h"

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
    printf("%s rev %d: %s\n", game.code, game.revision, game.title);
    frames(600);
    load_ram(argv[2], "ewram.bin", 0x02000000);
    load_ram(argv[2], "iwram.bin", 0x03000000);
    pd_mgba_reset();
    frames(60);

    check(pd_act_supported(&game), "the ROM's code is where the tables say");
    check(pd_act_can_pc(&game), "PC code found");
    check(pd_act_can_fly(&game), "FLY code found");
    const struct pd_config* cfg = game.cfg;
    bool emerald = game.kind == PD_GAME_EMERALD;
    uint32_t setMonData = emerald ? 0x0806ACAC : !strcmp(game.code, "BPGE") && game.revision == 0 ? 0x0804037C : 0x08040390;
    uint32_t scratch = 0x0203FFF0;

    struct pd_act a;
    memset(&a, 0, sizeof(a));
    stepping = &a;
    uint32_t lockFn = emerald ? 0x08098E6C : !strcmp(game.code, "BPGE") && game.revision == 0 ? 0x08069958 : 0x0806996C;
    if (call(lockFn, 0, 0, 0)) {
        // firered_vanilla was captured with the quest log's recap still on
        // (controls locked): the actions must say NOT_NOW. Then the recap's
        // state cleared (sLockFieldControls, gQuestLogState) for the rest.
        a.item = 13;
        run(&a, PD_REQ_USE);
        check(a.result == PD_ACT_NOT_NOW, "controls locked: NOT NOW");
        uint8_t zero = 0;
        io.write(core, 0x03000F9C, &zero, 1);
        io.write(core, 0x0203ADFA, &zero, 1);
        frames(2);
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
        call(emerald ? 0x080D6928 : !strcmp(game.code, "BPGE") ? (game.revision ? 0x0809A06C : 0x0809A058) : 0x0809A098,
             13, 1, 0);
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
        call(emerald ? 0x080D6928 : !strcmp(game.code, "BPGE") ? (game.revision ? 0x0809A06C : 0x0809A058) : 0x0809A098,
             86, 2, 0);
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
