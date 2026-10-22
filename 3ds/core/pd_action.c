#include "pd_action.h"

#include <stdio.h>
#include <string.h>

#include "pd_romart.h"
#include "pd_tables.h"

// The app's FieldItemFns + PartyItemFns + PcFns + FlyFns for one ROM. Every
// address from pret's decomp built clean (byte for byte retail) and its ELF;
// the CRCs are of each function's first 32 bytes, so a build that moved them
// is left alone (copied from FieldItems.kt / Pc.kt / Fly.kt - keep in sync).
struct fly_heal {
    uint32_t toHealLocation, toMapWarp, returnToField, saveBlock2Ptr;
    int specialMapsec[2], specialHeal[2];
    int genderMapsec, maleHeal, femaleHeal;
};

struct actions {
    const char* code;
    int revision;
    // The bag (FieldItemFns).
    uint32_t varGet, varSet, checkBagHasItem, removeBagItem, fieldControlsLocked;
    int repelVar;
    uint32_t items;
    uint32_t crcs[5];
    // The party (PartyItemFns).
    uint32_t getMonData, setMonData, itemNoEffect, itemEffect, addBagItem, party, partyCount, scratch;
    int itemCount, unholdable[2];
    uint32_t partyCrcs[5]; // getMonData, setMonData, itemNoEffect (if any), itemEffect, addBagItem
    uint32_t useFuncs[4];
    // The PC (PcFns).
    uint32_t setupScript;
    int special;
    uint32_t specials, pcFn, pcCrcs[2];
    // FLY (FlyFns).
    uint32_t flagGet;
    int badgeFlag;
    uint32_t setUpFieldMoveFly, getMapsecType, fadeScreen, lockControls, unlockControls, setFlyWarpDestination;
    uint32_t partyMenuSlotId, destinations;
    int firstMapsec, flyCount;
    uint32_t flyCrcs[9]; // the functions above; Emerald: its three heal-warp ones in place of setFlyWarpDestination
    int flyTypes[2], extraMapsec;
    const struct fly_heal* heal;
    // The field's main callback (CB2_Overworld): anything else is a menu, a
    // battle or a script's screen.
    uint32_t overworldCb2;
};

#define PC_SCRIPT 0x0203FFF4u

static const struct actions FIRERED_REV1 = {
    "BPRE", 1,
    0x0806E57C, 0x0806E598, 0x08099F54, 0x0809A1EC, 0x0806996C, 0x4020, 0x083DB098,
    { 0xE6DE47BA, 0x6E7CC2BE, 0xD30D656E, 0xC40BF208, 0x9C26CCA4 },
    0x0803FBFC, 0x08040390, 0x08042428, 0x080413D4, 0x0809A098, 0x02024284, 0x02024029, 0x0203FFF0,
    375, { 2, 4 },
    { 0xA73A32CD, 0x71F5A020, 0x788AB547, 0xEADAC6C9, 0xC316CD20 },
    { 0x080A16F4, 0x080A1710, 0x080A172C, 0x080A19AC },
    0x08069AF8, 60, 0x0815FDD8, 0x0808C6BD, { 0xAAC00946, 0x9662F022 },
    0x0806E6E4, 0x822, 0x08124B04, 0x080C35F0, 0x0807A82C, 0x08069954, 0x08069960, 0x080C5290,
    0x0203B0A9, 0x083F2F50, 0x58, 108,
    { 0x07BEBC3A, 0xCD8961DF, 0x348C5B9F, 0xF9959006, 0x0111A86E, 0xBAB654BA, 0x70EA1DFC },
    { 2, 2 }, -1, NULL,
    0x080565C9,
};

static const struct actions LEAFGREEN_REV0 = {
    "BPGE", 0,
    0x0806E568, 0x0806E584, 0x08099F14, 0x0809A1AC, 0x08069958, 0x4020, 0x083DAE64,
    { 0xE6DE47BA, 0x6E7CC2BE, 0xD30D656E, 0xC40BF208, 0x9C26CCA4 },
    0x0803FBE8, 0x0804037C, 0x08042414, 0x080413C0, 0x0809A058, 0x02024284, 0x02024029, 0x0203FFF0,
    375, { 2, 4 },
    { 0x7841F205, 0x9B11B0A6, 0x788AB547, 0xEADAC6C9, 0xC316CD20 },
    { 0x080A16B4, 0x080A16D0, 0x080A16EC, 0x080A196C },
    0x08069AE4, 60, 0x0815FD3C, 0x0808C67D, { 0xAAC00946, 0x5333E53E },
    0x0806E6D0, 0x822, 0x08124A64, 0x080C35B0, 0x0807A7EC, 0x08069940, 0x0806994C, 0x080C5250,
    0x0203B0A9, 0x083F2D1C, 0x58, 108,
    { 0x07BEBC3A, 0x5B68C93A, 0xE4D65E08, 0xF9959006, 0x0111A86E, 0xBAB654BA, 0x059815BF },
    { 2, 2 }, -1, NULL,
    0x080565B5,
};

static const struct actions LEAFGREEN_REV1 = {
    "BPGE", 1,
    0x0806E57C, 0x0806E598, 0x08099F28, 0x0809A1C0, 0x0806996C, 0x4020, 0x083DAED4,
    { 0xE6DE47BA, 0x6E7CC2BE, 0xD30D656E, 0xC40BF208, 0x9C26CCA4 },
    0x0803FBFC, 0x08040390, 0x08042428, 0x080413D4, 0x0809A06C, 0x02024284, 0x02024029, 0x0203FFF0,
    375, { 2, 4 },
    { 0xA73A32CD, 0x71F5A020, 0x788AB547, 0xEADAC6C9, 0xC316CD20 },
    { 0x080A16C8, 0x080A16E4, 0x080A1700, 0x080A1980 },
    0x08069AF8, 60, 0x0815FDB4, 0x0808C691, { 0xAAC00946, 0x5333E53E },
    0x0806E6E4, 0x822, 0x08124ADC, 0x080C35C4, 0x0807A800, 0x08069954, 0x08069960, 0x080C5264,
    0x0203B0A9, 0x083F2D8C, 0x58, 108,
    { 0x07BEBC3A, 0xDBE480BD, 0x50C0C136, 0xF9959006, 0x0111A86E, 0xBAB654BA, 0x059815BF },
    { 2, 2 }, -1, NULL,
    0x080565C9,
};

// Emerald has no SetFlyWarpDestination: CB_ExitFlyMap's heal location / map
// warp, then ReturnToFieldFromFlyMapSelect. Ever Grande flies to the city,
// Littleroot by the player's gender, the Battle Frontier past the table.
static const struct fly_heal EMERALD_HEAL = {
    0x08084CCC, 0x08084C28, 0x080B69DC, 0x03005D90,
    { 15, 58 }, { 11, 22 },
    0, 12, 13,
};

static const struct actions EMERALD = {
    "BPEE", 0,
    0x0809D694, 0x0809D6B0, 0x080D6724, 0x080D6AA4, 0x08098E6C, 0x4021, 0x085839A0,
    { 0x7D0D07B6, 0x519C39C0, 0x312F6576, 0x379EF742, 0x2532A407 },
    0x0806A518, 0x0806ACAC, 0, 0x0806BD04, 0x080D6928, 0x020244EC, 0x020244E9, 0x0203FFF0,
    377, { 5, 3 },
    { 0x9670EBDE, 0x8D555FF8, 0xEADAC6C9, 0x75BC5970 },
    { 0x080FDEA0, 0x080FDEF4, 0x080FDF10, 0x080FE0BC },
    0x08098EF8, 63, 0x081DBA64, 0x080C7579, { 0xEF3698BA, 0x81188148 },
    0x0809D790, 0x86C, 0x081B5884, 0x08123D58, 0x080ABCD0, 0x08098E54, 0x08098E60, 0,
    0x0203CED1, 0x085A1E3C, 0, 50,
    { 0xBCF97BF3, 0x13BE1A7A, 0x4B625BC4, 0xF9959006, 0xB9FB85A7, 0xB037C8AF, 0x74DBCE3A, 0x2F1D18CA, 0xA284286D },
    { 2, 4 }, 58, &EMERALD_HEAL,
    0x08085E5D,
};

static const struct actions* const ALL[] = { &FIRERED_REV1, &LEAFGREEN_REV0, &LEAFGREEN_REV1, &EMERALD };

#define ITEM_SIZE 44
#define ITEM_PARAM 0x13
#define ITEM_IMPORTANCE 0x18
#define ITEM_POCKET 0x1A
#define ITEM_FIELD_USE 0x1C
#define MON_SIZE 100
#define MON_DATA_PERSONALITY 0
#define MON_DATA_SPECIES 11
#define MON_DATA_HELD_ITEM 12
#define MON_DATA_MOVE1 13
#define MON_DATA_IS_EGG 45
#define HP_UP 63
#define SHEDINJA 303
#define MOVE_FLY 19
#define FLY_FADE_FRAMES 30
#define SAFE_TRIES 120 // frames a request waits for the game to be callable

static bool in_set(int v, const int* set, int n) {
    for (int i = 0; i < n; i++) {
        if (set[i] == v) return true;
    }
    return false;
}

static const int REPELS[] = { 86, 83, 84 };
static const int MEDICINE[] = { 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
                                38, 44, 63, 64, 65, 66, 67, 70, 133, 134, 135, 136, 137, 139, 141, 142 };
static const int PP_ONE[] = { 34, 35, 138, 69, 71 };
static const int PP_ALL[] = { 36, 37 };
#define N(a) ((int) (sizeof(a) / sizeof((a)[0])))

static bool is_repel(int item) { return in_set(item, REPELS, N(REPELS)); }
static bool is_mail(int item) { return item >= 121 && item <= 132; }
static bool is_party_use(int item) {
    return in_set(item, MEDICINE, N(MEDICINE)) || in_set(item, PP_ONE, N(PP_ONE)) || in_set(item, PP_ALL, N(PP_ALL));
}
bool pd_act_pp_boost(int item) { return item == 69 || item == 71; }

static uint32_t u32(const uint8_t* p) {
    return (uint32_t) p[0] | (uint32_t) p[1] << 8 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24;
}

static bool crc_ok(const struct pd_game* g, uint32_t fn, uint32_t crc) {
    const uint8_t* code = pd_rom_at(g, fn & ~1u, 32);
    return code && pd_crc32(code, 32) == crc;
}

// The ROM's tables, checked once per ROM: the bag's and party's code (the
// app's fieldItemCodeMatches), then the PC's and FLY's on their own.
struct support {
    unsigned rom;
    const struct actions* t;
    bool pc, fly;
};

static const struct support* support_for(const struct pd_game* g) {
    static struct support s;
    unsigned rom = pd_rom_id(g);
    if (s.rom == rom && rom) return &s;
    memset(&s, 0, sizeof(s));
    s.rom = rom;
    if (!rom || !g->cfg) return &s;
    for (size_t i = 0; i < sizeof(ALL) / sizeof(ALL[0]); i++) {
        const struct actions* t = ALL[i];
        if (strncmp(g->code, t->code, 4) || g->revision != t->revision) continue;
        const uint32_t fns[5] = { t->varGet, t->varSet, t->checkBagHasItem, t->removeBagItem, t->fieldControlsLocked };
        bool ok = true;
        for (int k = 0; k < 5 && ok; k++) ok = crc_ok(g, fns[k], t->crcs[k]);
        uint32_t party[5];
        int n = 0;
        party[n++] = t->getMonData;
        party[n++] = t->setMonData;
        if (t->itemNoEffect) party[n++] = t->itemNoEffect;
        party[n++] = t->itemEffect;
        party[n++] = t->addBagItem;
        for (int k = 0; k < n && ok; k++) ok = crc_ok(g, party[k], t->partyCrcs[k]);
        if (!ok) break;
        s.t = t;
        uint8_t entry[4];
        s.pc = pd_rom_read(g, t->specials + 4u * (uint32_t) t->special, entry, 4) && u32(entry) == t->pcFn &&
               crc_ok(g, t->setupScript, t->pcCrcs[0]) && crc_ok(g, t->pcFn, t->pcCrcs[1]);
        uint32_t fly[9] = { t->flagGet, t->setUpFieldMoveFly, t->getMapsecType, t->fadeScreen, t->lockControls,
                            t->unlockControls };
        int fn = 6;
        if (t->heal) {
            fly[fn++] = t->heal->toHealLocation;
            fly[fn++] = t->heal->toMapWarp;
            fly[fn++] = t->heal->returnToField;
        } else {
            fly[fn++] = t->setFlyWarpDestination;
        }
        s.fly = true;
        for (int k = 0; k < fn && s.fly; k++) s.fly = crc_ok(g, fly[k], t->flyCrcs[k]);
        break;
    }
    return &s;
}

bool pd_act_supported(const struct pd_game* g) {
    return support_for(g)->t != NULL;
}

static const uint8_t* item_record(const struct pd_game* g, const struct actions* t, int item) {
    if (item <= 0 || item >= t->itemCount) return NULL;
    return pd_rom_at(g, t->items + (uint32_t) item * ITEM_SIZE, ITEM_SIZE);
}

// USE's items whose gItems entry runs one of the game's field-use routines
// for medicine / PP / Repels (the app's usableItems): a moved id stays off.
bool pd_act_can_use(const struct pd_game* g, int item) {
    const struct actions* t = support_for(g)->t;
    if (!t || !(is_repel(item) || is_party_use(item))) return false;
    const uint8_t* r = item_record(g, t, item);
    if (!r) return false;
    uint32_t fn = u32(r + ITEM_FIELD_USE) & ~1u;
    for (int i = 0; i < 4; i++) {
        if (t->useFuncs[i] == fn) return true;
    }
    return false;
}

// GIVE: importance 0, not a key item / TM, not Mail (the app's holdableItems).
bool pd_act_can_give(const struct pd_game* g, int item) {
    const struct actions* t = support_for(g)->t;
    const uint8_t* r = t ? item_record(g, t, item) : NULL;
    return r && r[ITEM_IMPORTANCE] == 0 && !in_set(r[ITEM_POCKET], t->unholdable, 2) && !is_mail(item);
}

bool pd_act_picks_mon(const struct pd_game* g, int item) {
    return pd_act_can_use(g, item) && !is_repel(item);
}

bool pd_act_picks_move(const struct pd_game* g, int item) {
    return pd_act_can_use(g, item) && in_set(item, PP_ONE, N(PP_ONE));
}

bool pd_act_can_take(const struct pd_game* g) { return support_for(g)->t != NULL; }
bool pd_act_can_pc(const struct pd_game* g) { return support_for(g)->pc; }
bool pd_act_can_fly(const struct pd_game* g) { return support_for(g)->fly; }

void pd_act_request(struct pd_act* a, enum pd_act_kind kind) {
    a->kind = kind;
    a->tries = 0;
}

// --- the calls ---------------------------------------------------------------

// A call that never returned is an error the whole request gives up on.
struct ctx {
    const struct pd_game* g;
    const struct pd_game_io* io;
    const struct actions* t;
    bool failed;
};

static uint32_t call(struct ctx* c, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    if (c->failed) return 0;
    const uint32_t args[4] = { a0, a1, a2, a3 };
    uint32_t r0 = 0;
    if (!c->io->call(c->io->ctx, fn, args, &r0)) c->failed = true;
    return r0;
}

static uint8_t read8(struct ctx* c, uint32_t addr) {
    uint8_t v = 0;
    if (!c->io->read(c->io->ctx, addr, &v, 1)) c->failed = true;
    return v;
}

static uint32_t mon(const struct actions* t, int slot) {
    return t->party + (uint32_t) slot * MON_SIZE;
}

static uint32_t get(struct ctx* c, int slot, int field) {
    return call(c, c->t->getMonData, mon(c->t, slot), (uint32_t) field, 0, 0);
}

static int party_count(struct ctx* c) {
    int n = read8(c, c->t->partyCount);
    return n > PD_PARTY_SIZE ? PD_PARTY_SIZE : n;
}

static bool locked(struct ctx* c) {
    return call(c, c->t->fieldControlsLocked, 0, 0, 0, 0) != 0;
}

// Whether USE of item on slot would do something (move for a PP item): the
// game's own no-effect check, or on Emerald the effect itself, undone.
static bool helps(struct ctx* c, int item, int slot, int move) {
    const struct actions* t = c->t;
    if (item == HP_UP && get(c, slot, MON_DATA_SPECIES) == SHEDINJA) return false;
    if (t->itemNoEffect) return call(c, t->itemNoEffect, mon(t, slot), (uint32_t) item, (uint32_t) slot, (uint32_t) move) == 0;
    uint8_t saved[MON_SIZE];
    if (!c->io->read(c->io->ctx, mon(t, slot), saved, MON_SIZE)) {
        c->failed = true;
        return false;
    }
    bool did = call(c, t->itemEffect, mon(t, slot), (uint32_t) item, (uint32_t) slot, (uint32_t) move) == 0;
    if (!c->io->write(c->io->ctx, mon(t, slot), saved, MON_SIZE)) c->failed = true;
    return did;
}

static bool still_there(struct ctx* c, int slot, uint32_t personality) {
    return slot >= 0 && slot < party_count(c) && get(c, slot, MON_DATA_PERSONALITY) == personality &&
           get(c, slot, MON_DATA_IS_EGG) == 0;
}

// SetMonData(mon, MON_DATA_HELD_ITEM, &item) through the scratch word, read back.
static bool set_held(struct ctx* c, int slot, int item) {
    const struct actions* t = c->t;
    uint8_t before[2], v[2] = { (uint8_t) item, (uint8_t) (item >> 8) };
    if (!c->io->read(c->io->ctx, t->scratch, before, 2) || !c->io->write(c->io->ctx, t->scratch, v, 2)) return false;
    call(c, t->setMonData, mon(t, slot), MON_DATA_HELD_ITEM, t->scratch, 0);
    c->io->write(c->io->ctx, t->scratch, before, 2);
    return !c->failed && get(c, slot, MON_DATA_HELD_ITEM) == (uint32_t) item;
}

// Whether the bag has room for one more item: the game's AddBagItem, then EWRAM
// put back byte for byte (the app's bagHasRoom). -1 = couldn't tell.
#define EWRAM 0x02000000u
#define EWRAM_SIZE 0x40000u
static int bag_has_room(struct ctx* c, int item) {
    static uint8_t before[EWRAM_SIZE], after[EWRAM_SIZE];
    if (!c->io->read(c->io->ctx, EWRAM, before, EWRAM_SIZE)) return -1;
    uint32_t added = call(c, c->t->addBagItem, (uint32_t) item, 1, 0, 0);
    bool ok = c->io->read(c->io->ctx, EWRAM, after, EWRAM_SIZE);
    for (uint32_t i = 0; i < EWRAM_SIZE;) {
        if (ok && after[i] == before[i]) {
            i++;
            continue;
        }
        uint32_t j = i;
        while (j < EWRAM_SIZE && (!ok || after[j] != before[j])) j++;
        if (!c->io->write(c->io->ctx, EWRAM + i, before + i, j - i)) c->failed = true;
        i = j;
    }
    return c->failed ? -1 : added == 1;
}

static enum pd_act_result use_repel(struct ctx* c, struct pd_act* a) {
    const struct actions* t = c->t;
    int item = a->item;
    if (locked(c)) return PD_ACT_NOT_NOW;
    if (call(c, t->varGet, (uint32_t) t->repelVar, 0, 0, 0) != 0) return PD_ACT_STILL_ACTIVE;
    if (call(c, t->checkBagHasItem, (uint32_t) item, 1, 0, 0) != 1) return PD_ACT_NONE_LEFT;
    const uint8_t* r = item_record(c->g, t, item);
    int steps = r ? r[ITEM_PARAM] : 0;
    if (!steps || c->failed) return PD_ACT_FAILED;
    call(c, t->varSet, (uint32_t) t->repelVar, (uint32_t) steps, 0, 0);
    if (call(c, t->removeBagItem, (uint32_t) item, 1, 0, 0) != 1) return PD_ACT_FAILED;
    a->steps = steps;
    return PD_ACT_USED;
}

// USE / GIVE's party pick: every slot with its verdict (the app's partyTargets).
static enum pd_act_result targets(struct ctx* c, struct pd_act* a, bool give) {
    const struct actions* t = c->t;
    int item = a->item;
    if (locked(c)) return PD_ACT_NOT_NOW;
    if (call(c, t->checkBagHasItem, (uint32_t) item, 1, 0, 0) != 1) return PD_ACT_NONE_LEFT;
    int n = party_count(c);
    bool oneMove = in_set(item, PP_ONE, N(PP_ONE));
    for (int slot = 0; slot < n && !c->failed; slot++) {
        struct pd_target* x = &a->targets[slot];
        memset(x, 0, sizeof(*x));
        x->personality = get(c, slot, MON_DATA_PERSONALITY);
        x->heldItem = (int) get(c, slot, MON_DATA_HELD_ITEM);
        if (get(c, slot, MON_DATA_IS_EGG)) x->verdict = PD_SLOT_EGG;
        else if (give && is_mail(x->heldItem)) x->verdict = PD_SLOT_HOLDS_MAIL;
        else if (give && x->heldItem == item) x->verdict = PD_SLOT_HOLDS_IT;
        else if (give) x->verdict = PD_SLOT_OK;
        else if (oneMove) {
            bool any = false;
            for (int m = 0; m < 4; m++) {
                x->moves[m] = get(c, slot, MON_DATA_MOVE1 + m) != 0 && helps(c, item, slot, m);
                any |= x->moves[m];
            }
            x->verdict = any ? PD_SLOT_OK : PD_SLOT_NO_EFFECT;
        } else {
            x->verdict = helps(c, item, slot, 0) ? PD_SLOT_OK : PD_SLOT_NO_EFFECT;
        }
    }
    a->targetCount = n;
    return c->failed ? PD_ACT_FAILED : PD_ACT_USED;
}

static enum pd_act_result use_on(struct ctx* c, struct pd_act* a) {
    const struct actions* t = c->t;
    int item = a->item, slot = a->slot;
    int m = in_set(item, PP_ONE, N(PP_ONE)) ? (a->move < 0 ? 0 : a->move > 3 ? 3 : a->move) : 0;
    if (locked(c)) return PD_ACT_NOT_NOW;
    if (call(c, t->checkBagHasItem, (uint32_t) item, 1, 0, 0) != 1) return PD_ACT_NONE_LEFT;
    if (!still_there(c, slot, a->personality)) return c->failed ? PD_ACT_FAILED : PD_ACT_PARTY_CHANGED;
    if (!helps(c, item, slot, m)) return c->failed ? PD_ACT_FAILED : PD_ACT_NO_EFFECT;
    if (call(c, t->itemEffect, mon(t, slot), (uint32_t) item, (uint32_t) slot, (uint32_t) m) != 0) return PD_ACT_NO_EFFECT;
    if (call(c, t->removeBagItem, (uint32_t) item, 1, 0, 0) != 1) return PD_ACT_FAILED;
    return c->failed ? PD_ACT_FAILED : PD_ACT_USED;
}

// GIVE, the party menu's way: never loses an item (the app's giveToMon).
static enum pd_act_result give_to(struct ctx* c, struct pd_act* a) {
    const struct actions* t = c->t;
    int item = a->item, slot = a->slot;
    if (locked(c)) return PD_ACT_NOT_NOW;
    if (call(c, t->checkBagHasItem, (uint32_t) item, 1, 0, 0) != 1) return PD_ACT_NONE_LEFT;
    if (!still_there(c, slot, a->personality)) return c->failed ? PD_ACT_FAILED : PD_ACT_PARTY_CHANGED;
    int old = (int) get(c, slot, MON_DATA_HELD_ITEM);
    if (c->failed || is_mail(old)) return PD_ACT_FAILED;
    if (old == item) return PD_ACT_NO_EFFECT;
    if (call(c, t->removeBagItem, (uint32_t) item, 1, 0, 0) != 1) return PD_ACT_NONE_LEFT;
    if (old && call(c, t->addBagItem, (uint32_t) old, 1, 0, 0) != 1) {
        call(c, t->addBagItem, (uint32_t) item, 1, 0, 0);
        a->swapped = old;
        return PD_ACT_BAG_FULL;
    }
    if (set_held(c, slot, item)) {
        a->swapped = old;
        return PD_ACT_GIVEN;
    }
    // Back as it was.
    if (old && call(c, t->removeBagItem, (uint32_t) old, 1, 0, 0) == 1) set_held(c, slot, old);
    call(c, t->addBagItem, (uint32_t) item, 1, 0, 0);
    return PD_ACT_FAILED;
}

// TAKE (the app's takeFromMon).
static enum pd_act_result take(struct ctx* c, struct pd_act* a) {
    const struct actions* t = c->t;
    int slot = a->slot;
    if (locked(c)) return PD_ACT_NOT_NOW;
    if (!still_there(c, slot, a->personality)) return c->failed ? PD_ACT_FAILED : PD_ACT_PARTY_CHANGED;
    int held = (int) get(c, slot, MON_DATA_HELD_ITEM);
    a->swapped = held;
    if (!held) return PD_ACT_NO_EFFECT;
    if (is_mail(held) || c->failed) return PD_ACT_FAILED;
    if (bag_has_room(c, held) == 0) return PD_ACT_BAG_FULL;
    if (call(c, t->addBagItem, (uint32_t) held, 1, 0, 0) != 1) return PD_ACT_BAG_FULL;
    if (set_held(c, slot, 0)) return PD_ACT_TAKEN;
    call(c, t->removeBagItem, (uint32_t) held, 1, 0, 0);
    return PD_ACT_FAILED;
}

// lockall; special N; waitstate; releaseall; end - run by the game's script engine.
static enum pd_act_result open_pc(struct ctx* c) {
    const struct actions* t = c->t;
    if (locked(c)) return PD_ACT_NOT_NOW;
    const uint8_t script[7] = { 0x69, 0x25, (uint8_t) t->special, (uint8_t) (t->special >> 8), 0x27, 0x6B, 0x02 };
    if (!c->io->write(c->io->ctx, PC_SCRIPT, script, sizeof(script))) return PD_ACT_FAILED;
    call(c, t->setupScript, PC_SCRIPT, 0, 0, 0);
    return c->failed ? PD_ACT_FAILED : PD_ACT_USED;
}

// --- FLY -----------------------------------------------------------------------

static bool fly_type(const struct actions* t, int type) {
    return type == t->flyTypes[0] || type == t->flyTypes[1];
}

static bool known_mapsec(const struct actions* t, int mapsec) {
    return (mapsec - t->firstMapsec >= 0 && mapsec - t->firstMapsec < t->flyCount) || mapsec == t->extraMapsec;
}

static enum pd_act_result fly_gate(struct ctx* c) {
    const struct actions* t = c->t;
    if (locked(c)) return PD_ACT_NOT_NOW;
    if (call(c, t->flagGet, (uint32_t) t->badgeFlag, 0, 0, 0) != 1) return PD_ACT_NO_BADGE;
    if (call(c, t->setUpFieldMoveFly, 0, 0, 0, 0) != 1) return PD_ACT_CANT_FLY_HERE;
    return c->failed ? PD_ACT_FAILED : PD_ACT_NONE;
}

// The first party Pokémon that knows FLY (not an egg), -1 = none.
static int flyer(struct ctx* c, uint32_t* personality) {
    int n = party_count(c);
    for (int slot = 0; slot < n && !c->failed; slot++) {
        if (get(c, slot, MON_DATA_IS_EGG)) continue;
        for (int m = 0; m < 4; m++) {
            if (get(c, slot, MON_DATA_MOVE1 + m) == MOVE_FLY) {
                *personality = get(c, slot, MON_DATA_PERSONALITY);
                return slot;
            }
        }
    }
    return -1;
}

static enum pd_act_result fly_targets(struct ctx* c, struct pd_act* a) {
    const struct actions* t = c->t;
    enum pd_act_result r = fly_gate(c);
    if (r != PD_ACT_NONE) return r;
    a->flyerSlot = flyer(c, &a->flyerPersonality);
    if (a->flyerSlot < 0) return c->failed ? PD_ACT_FAILED : PD_ACT_NO_FLYER;
    const uint8_t* table = pd_rom_at(c->g, t->destinations, (size_t) t->flyCount * 3);
    if (!table) return PD_ACT_FAILED;
    a->flyCount = 0;
    for (int i = 0; i <= t->flyCount && a->flyCount < PD_FLY_MAX; i++) {
        int mapsec;
        if (i < t->flyCount) {
            if (!(table[i * 3] | table[i * 3 + 1] | table[i * 3 + 2])) continue;
            mapsec = t->firstMapsec + i;
        } else if (t->extraMapsec >= 0) {
            mapsec = t->extraMapsec;
        } else {
            break;
        }
        if (fly_type(t, (int) call(c, t->getMapsecType, (uint32_t) mapsec, 0, 0, 0))) {
            a->fly[a->flyCount++] = (uint16_t) mapsec;
        }
    }
    return c->failed ? PD_ACT_FAILED : PD_ACT_USED;
}

// FLY's first step: the checks again, then what closing the Fly map leaves -
// controls locked, the screen fading to black. The second step follows.
static enum pd_act_result fly_start(struct ctx* c, struct pd_act* a) {
    const struct actions* t = c->t;
    enum pd_act_result r = fly_gate(c);
    if (r != PD_ACT_NONE) return r;
    if (!known_mapsec(t, a->mapsec) || !fly_type(t, (int) call(c, t->getMapsecType, (uint32_t) a->mapsec, 0, 0, 0))) {
        return PD_ACT_FAILED;
    }
    uint32_t personality = 0;
    int slot = flyer(c, &personality);
    if (slot < 0 || slot != a->flyerSlot || personality != a->flyerPersonality) return PD_ACT_NO_FLYER;
    call(c, t->lockControls, 0, 0, 0, 0);
    call(c, t->fadeScreen, 1, 0, 0, 0);
    if (c->failed) return PD_ACT_FAILED;
    a->flyWait = FLY_FADE_FRAMES;
    a->flyMapsec = a->mapsec;
    return PD_ACT_FLYING;
}

static bool warp(struct ctx* c, struct pd_act* a) {
    const struct actions* t = c->t;
    int mapsec = a->flyMapsec;
    const struct fly_heal* h = t->heal;
    if (!h) {
        call(c, t->setFlyWarpDestination, (uint32_t) mapsec, 0, 0, 0);
        return !c->failed;
    }
    int i = mapsec - t->firstMapsec;
    uint8_t entry[3] = { 0 };
    if (i >= 0 && i < t->flyCount) pd_rom_read(c->g, t->destinations + (uint32_t) i * 3, entry, 3);
    int heal = entry[2];
    for (int k = 0; k < 2; k++) {
        if (h->specialMapsec[k] == mapsec) heal = h->specialHeal[k];
    }
    if (mapsec == h->genderMapsec) {
        uint8_t w[4];
        if (!c->io->read(c->io->ctx, h->saveBlock2Ptr, w, 4)) return false;
        heal = read8(c, u32(w) + 8) == 0 ? h->maleHeal : h->femaleHeal;
    }
    if (heal) call(c, h->toHealLocation, (uint32_t) heal, 0, 0, 0);
    else if (entry[0] | entry[1]) call(c, h->toMapWarp, entry[0], entry[1], 0xFFFFFFFFu, 0);
    else return false;
    call(c, h->returnToField, 0, 0, 0, 0);
    return !c->failed;
}

// FLY's second step: the cut-in's Pokémon under the party cursor, then the Fly
// map's choice - unless the game left the field during the fade (then nothing:
// whatever took it off brings it back). Fails back to the field faded in.
static void fly_finish(struct ctx* c, struct pd_act* a, bool onField) {
    const struct actions* t = c->t;
    if (!onField) return;
    uint8_t slot = (uint8_t) a->flyerSlot;
    bool ok = c->io->write(c->io->ctx, t->partyMenuSlotId, &slot, 1) && warp(c, a);
    if (!ok) {
        c->failed = false;
        call(c, t->fadeScreen, 0, 0, 0, 0);
        call(c, t->unlockControls, 0, 0, 0, 0);
    }
}

// --- the host's step -------------------------------------------------------------

static bool on_field(const struct pd_game* g, const struct pd_game_io* io, const struct actions* t) {
    uint8_t w[4];
    return g->cfg && io->read(io->ctx, g->cfg->gMain + 4, w, 4) && u32(w) == t->overworldCb2;
}

bool pd_act_step(struct pd_act* a, const struct pd_game* g, const struct pd_game_io* io, bool inBattle) {
    const struct support* s = support_for(g);
    if (a->flyWait > 0 && --a->flyWait == 0 && s->t) {
        struct ctx c = { g, io, s->t, false };
        fly_finish(&c, a, on_field(g, io, s->t));
    }
    if (a->kind == PD_REQ_NONE) return false;
    enum pd_act_kind kind = a->kind;
    enum pd_act_result r = PD_ACT_FAILED;
    if (!s->t || (kind == PD_REQ_PC && !s->pc) || ((kind == PD_REQ_FLY || kind == PD_REQ_FLY_TARGETS) && !s->fly)) {
        r = PD_ACT_FAILED;
    } else if (inBattle) {
        r = PD_ACT_IN_BATTLE;
    } else if (!on_field(g, io, s->t)) {
        r = PD_ACT_NOT_NOW;
    } else {
        // The game must be callable now (between its frames, not mid-way
        // through its own code): else the request waits for a frame that is.
        const uint32_t none[4] = { 0 };
        uint32_t r0;
        if (!io->call(io->ctx, s->t->fieldControlsLocked, none, &r0)) {
            if (++a->tries < SAFE_TRIES) return false;
            r = PD_ACT_NOT_NOW;
            goto done;
        }
        struct ctx c = { g, io, s->t, false };
        switch (kind) {
        case PD_REQ_USE:
            if (!pd_act_can_use(g, a->item)) r = PD_ACT_FAILED;
            else r = is_repel(a->item) ? use_repel(&c, a) : targets(&c, a, false);
            break;
        case PD_REQ_GIVE_TARGETS: r = pd_act_can_give(g, a->item) ? targets(&c, a, true) : PD_ACT_FAILED; break;
        case PD_REQ_USE_ON: r = pd_act_picks_mon(g, a->item) ? use_on(&c, a) : PD_ACT_FAILED; break;
        case PD_REQ_GIVE_TO: r = pd_act_can_give(g, a->item) ? give_to(&c, a) : PD_ACT_FAILED; break;
        case PD_REQ_TAKE: r = take(&c, a); break;
        case PD_REQ_PC: r = open_pc(&c); break;
        case PD_REQ_FLY_TARGETS: r = fly_targets(&c, a); break;
        case PD_REQ_FLY: r = fly_start(&c, a); break;
        default: break;
        }
        if (c.failed) r = PD_ACT_FAILED;
    }
done:
    a->kind = PD_REQ_NONE;
    a->done = kind;
    a->result = r;
    return true;
}

const char* pd_act_message(const struct pd_act* a, const struct pd_game* g, const struct pd_snapshot* s, char* buf,
                           size_t len) {
    const char* mon = a->slot >= 0 && a->slot < s->partyCount ? s->party[a->slot].nickname : "";
    switch (a->result) {
    case PD_ACT_USED:
        if (a->done == PD_REQ_PC || a->done == PD_REQ_FLY_TARGETS || a->done == PD_REQ_GIVE_TARGETS) return "";
        if (a->done == PD_REQ_USE && a->steps) snprintf(buf, len, "Used! It lasts %d steps.", a->steps);
        else if (a->done == PD_REQ_USE) return "";
        else snprintf(buf, len, "Used on %s.", mon);
        break;
    case PD_ACT_STILL_ACTIVE: snprintf(buf, len, "The last one is still working."); break;
    case PD_ACT_NOT_NOW: snprintf(buf, len, "Not now: close the game's menus and battles first."); break;
    case PD_ACT_IN_BATTLE: snprintf(buf, len, "You can't do that during a battle."); break;
    case PD_ACT_NONE_LEFT: snprintf(buf, len, "None left in the bag."); break;
    case PD_ACT_NO_EFFECT: snprintf(buf, len, a->done == PD_REQ_TAKE ? "%s isn't holding anything." : "It won't have any effect.", mon); break;
    case PD_ACT_GIVEN:
        if (a->swapped) {
            snprintf(buf, len, "%s is holding %s now. %s went back to the bag.", mon, pd_item_name(g, a->item),
                     pd_item_name(g, a->swapped));
        } else {
            snprintf(buf, len, "%s is holding %s now.", mon, pd_item_name(g, a->item));
        }
        break;
    case PD_ACT_BAG_FULL: snprintf(buf, len, "The bag is full: %s can't go back in it.", pd_item_name(g, a->swapped)); break;
    case PD_ACT_PARTY_CHANGED: snprintf(buf, len, "The party has changed. Try again."); break;
    case PD_ACT_TAKEN: snprintf(buf, len, "Received %s from %s.", pd_item_name(g, a->swapped), mon); break;
    case PD_ACT_NO_BADGE: snprintf(buf, len, "FLY needs a new BADGE first."); break;
    case PD_ACT_CANT_FLY_HERE: snprintf(buf, len, "You can't FLY from here."); break;
    case PD_ACT_NO_FLYER: snprintf(buf, len, "Nobody in the party knows FLY."); break;
    case PD_ACT_FLYING: return "";
    case PD_ACT_FAILED: snprintf(buf, len, "It can't be used here."); break;
    default: return "";
    }
    return buf;
}
