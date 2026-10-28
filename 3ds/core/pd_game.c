#include "pd_game.h"

#include <string.h>

#include "pd_sha1.h"
#include "pd_tables.h"

// NATIVE_FIRERED_REV0 / _REV1 share every RAM address and save offset below,
// and LeafGreen's maps put each RAM global at FireRed's address for the same
// revision (NATIVE_LEAFGREEN_REV0/1).
#define FIRERED_RAM                                                                                     \
    .playerParty = 0x02024284,                                                                          \
    .playerPartyCount = 0x02024029,                                                                     \
    .battleMons = 0x02023BE4,                                                                           \
    .battlerPositions = 0x02023BD6,                                                                     \
    .battlersCount = 0x02023BCC,                                                                        \
    .battleTypeFlags = 0x02022B4C,                                                                      \
    .gMain = 0x030030F0,                                                                                \
    .saveBlock1Ptr = 0x03005008,                                                                        \
    .saveBlock2Ptr = 0x0300500C,                                                                        \
    .mapHeader = 0x02036DFC,                                                                            \
    .encryptionKeyOff = 0xF20,                                                                          \
    .moneyOff = 0x290,                                                                                  \
    /* FireRed's item.c: Items, Key Items, Poke Balls, TM Case, Berry Pouch. */                         \
    .bagPockets = 0x0203988C,                                                                           \
    .bagOrder = { PD_POCKET_ITEMS, PD_POCKET_KEY, PD_POCKET_BALLS, PD_POCKET_TMHM, PD_POCKET_BERRIES }, \
    /* FIRERED_DEX_FLAGS (Pokedex.kt) and GuideTables' SaveBlock1 offsets. */                            \
    .dexSeenCopy1 = 0x5F8,                                                                              \
    .dexSeenCopy2 = 0x3A18,                                                                             \
    .nationalMagicOff = 3,                                                                              \
    .nationalMagic = 0xB9,                                                                              \
    .flagsOff = 0xEE0,                                                                                  \
    .varsOff = 0x1000,                                                                                  \
    .flagBytes = 0x120

// The FOE TEAM, from the rev 1 map (NATIVE_FIRERED_REV1; LeafGreen rev 1's map
// has the same addresses). monToSwitchIntoId sits at 0x5C in struct BattleStruct.
#define FIRERED_REV1_FOES        \
    .enemyParty = 0x0202402C,          \
    .battlerPartyIndexes = 0x02023BCE, \
    .battleStructPtr = 0x02023FE8,     \
    .monToSwitchIntoOff = 0x5C

// Battle input: pokefirered's battle_controller_player.c handlers, rev 1 (and
// LeafGreen rev 1) and LeafGreen rev 0 (each the map's first of the name);
// the cursors and gPartyMenu sit at the same RAM in all three.
#define FIRERED_INPUT_RAM                     \
    .battlerControllerFuncs = 0x03004FE0,      \
    .actionCursor = 0x02023FF8,               \
    .moveCursor = 0x02023FFC,                 \
    .partyMenu = 0x0203B0A0
#define FIRERED_REV1_INPUT                    \
    FIRERED_INPUT_RAM,                        \
    .inputAction = 0x0802E44C,                \
    .inputMove = 0x0802EA24,                  \
    .inputTarget = 0x0802E688,                \
    .inputItem = 0x08030750,                  \
    .inputParty = 0x08030698

// TRAINER_CARD_FIRERED: retail FireRed rev 1 and LeafGreen (the app has no
// card for FireRed rev 0).
#define FIRERED_CARD                \
    .cardStyle = PD_CARD_KANTO,      \
    .gameStatsOff = 0x1200,          \
    .badgeFlag = 0x820,              \
    .pokedexFlag = 0x829,            \
    .nationalVar = 0x404E,           \
    .nationalVarValue = 0x6258,      \
    .nationalFlag = 0x840,           \
    .stickerVar = 0x4049

// FireRed rev 1 (sha1 dd5945db..., what pret's firered_rev1 builds): its ROM
// tables too - POKEDEX_FIRERED_REV1 and GUIDE_TABLES_FIRERED_REV1.
static const struct pd_config FIRERED_REV1 = {
    FIRERED_RAM,
    FIRERED_REV1_FOES,
    FIRERED_CARD,
    FIRERED_REV1_INPUT,
    .items = 0x083DB098,
    .itemIcons = 0x083D4304,
    .monIconTable = 0x083D3810,
    .monIconPaletteIndices = 0x083D3EF0,
    .monIconPaletteTable = 0x083D40A8,
    .dexEntries = 0x0844E8B0,
    .dexEntryStride = 0x24,
    .speciesInfo = 0x082547F4,
    .evolutions = 0x082597C4,
    .tmhmLearnsets = 0x08252C38,
    .tmhmMoves = 0x0845A604,
    .footprints = 0x0843FB20,
    .mapGroups = 0x08352718,
    .speciesToNational = 0x0825205E,
    .abilityNames = 0x0824FCB0,
    .frontPics = 0x0823511C,
    .palettes = 0x0823737C,
    .regionalOrder = 0,
    .regionalCount = 151,
    .regionName = "KANTO",
    .trainers = 0x0823EB38,
    .learnsets = 0x0825D824,
    .wildHeaders = 0x083C9D28,
    .probeTrainer = 414,
    .probeName = "BROCK",
};

// FireRed rev 0: the tabs that read RAM (and the MAP, whose art is found by
// fingerprint) and the party icons. The app has no POKéDEX / GUIDE tables or
// FOE TEAM addresses for it either (NATIVE_FIRERED_REV0), nor a card.
static const struct pd_config FIRERED_REV0_RAM_ONLY = {
    FIRERED_RAM,
    .items = 0x083DB028,
    .itemIcons = 0x083D4294,
    .monIconTable = 0x083D37A0,
    .monIconPaletteIndices = 0x083D3E80,
    .monIconPaletteTable = 0x083D4038,
};

// LeafGreen (NATIVE_LEAFGREEN_REV0 / _REV1): FireRed's RAM for the same
// revision, its own ROM tables - POKEDEX_LEAFGREEN_REV0/1 and
// GUIDE_TABLES_LEAFGREEN_REV0/1 (BROCK is trainer 414 there too). The FOE
// TEAM's addresses are rev 1's, like FireRed's.
static const struct pd_config LEAFGREEN_REV0 = {
    FIRERED_RAM,
    FIRERED_CARD,
    FIRERED_INPUT_RAM,
    .inputAction = 0x0802E438,
    .inputMove = 0x0802EA10,
    .inputTarget = 0x0802E674,
    .inputItem = 0x0803073C,
    .inputParty = 0x08030684,
    .items = 0x083DAE64,
    .itemIcons = 0x083D40D0,
    .monIconTable = 0x083D35DC,
    .monIconPaletteIndices = 0x083D3CBC,
    .monIconPaletteTable = 0x083D3E74,
    .dexEntries = 0x0844E270,
    .dexEntryStride = 0x24,
    .speciesInfo = 0x08254760,
    .evolutions = 0x08259734,
    .tmhmLearnsets = 0x08252BA4,
    .tmhmMoves = 0x08459FC4,
    .footprints = 0x0843F8EC,
    .mapGroups = 0x08352688,
    .speciesToNational = 0x08251FCA,
    .abilityNames = 0x0824FC1C,
    .frontPics = 0x08235088,
    .palettes = 0x082372E8,
    .regionalOrder = 0,
    .regionalCount = 151,
    .regionName = "KANTO",
    .trainers = 0x0823EAA4,
    .learnsets = 0x0825D794,
    .wildHeaders = 0x083C9AF4,
    .probeTrainer = 414,
    .probeName = "BROCK",
};

static const struct pd_config LEAFGREEN_REV1 = {
    FIRERED_RAM,
    FIRERED_REV1_FOES,
    FIRERED_CARD,
    FIRERED_REV1_INPUT,
    .items = 0x083DAED4,
    .itemIcons = 0x083D4140,
    .monIconTable = 0x083D364C,
    .monIconPaletteIndices = 0x083D3D2C,
    .monIconPaletteTable = 0x083D3EE4,
    .dexEntries = 0x0844E2E0,
    .dexEntryStride = 0x24,
    .speciesInfo = 0x082547D0,
    .evolutions = 0x082597A4,
    .tmhmLearnsets = 0x08252C14,
    .tmhmMoves = 0x0845A034,
    .footprints = 0x0843F95C,
    .mapGroups = 0x083526F8,
    .speciesToNational = 0x0825203A,
    .abilityNames = 0x0824FC8C,
    .frontPics = 0x082350F8,
    .palettes = 0x08237358,
    .regionalOrder = 0,
    .regionalCount = 151,
    .regionName = "KANTO",
    .trainers = 0x0823EB14,
    .learnsets = 0x0825D804,
    .wildHeaders = 0x083C9B64,
    .probeTrainer = 414,
    .probeName = "BROCK",
};

// NATIVE_EMERALD_RETAIL (English, BPEE; sha1 f3ae0881..., what pret's
// pokeemerald builds), with POKEDEX_EMERALD and GUIDE_TABLES_EMERALD.
static const struct pd_config EMERALD = {
    .playerParty = 0x020244EC,
    .playerPartyCount = 0x020244E9,
    .battleMons = 0x02024084,
    .battlerPositions = 0x02024076,
    .battlersCount = 0x0202406C,
    .battleTypeFlags = 0x02022FEC,
    .gMain = 0x030022C0,
    .saveBlock1Ptr = 0x03005D8C,
    .saveBlock2Ptr = 0x03005D90,
    .mapHeader = 0x02037318,
    .encryptionKeyOff = 0xAC,
    .moneyOff = 0x490,
    .enemyParty = 0x02024744,
    .battlerPartyIndexes = 0x0202406E,
    .battleStructPtr = 0x0202449C,
    .monToSwitchIntoOff = 0x5C,
    .monIconTable = 0x0857BCA8,
    .monIconPaletteIndices = 0x0857C388,
    .monIconPaletteTable = 0x0857C540,
    // Emerald's item.c: Items, Poke Balls, TMs & HMs, Berries, Key Items.
    .bagPockets = 0x02039DD8,
    .items = 0x085839A0,
    .battlerControllerFuncs = 0x03005D60,
    .inputAction = 0x08057588,
    .inputMove = 0x08057BFC,
    .inputTarget = 0x08057824,
    .inputItem = 0x080598E0,
    .inputParty = 0x08059828,
    .actionCursor = 0x020244AC,
    .moveCursor = 0x020244B0,
    .partyMenu = 0x0203CEC8,
    .itemIcons = 0x08614410,
    .bagOrder = { PD_POCKET_ITEMS, PD_POCKET_BALLS, PD_POCKET_TMHM, PD_POCKET_BERRIES, PD_POCKET_KEY },
    .dexEntries = 0x0856B5B0,
    .dexEntryStride = 0x20,
    .speciesInfo = 0x083203CC,
    .evolutions = 0x0832531C,
    .tmhmLearnsets = 0x0831E898,
    .tmhmMoves = 0x08615B94,
    .footprints = 0x0856E694,
    .mapGroups = 0x08486578,
    .speciesToNational = 0x0831DC82,
    .abilityNames = 0x0831B6DB,
    .frontPics = 0x0830A18C,
    .palettes = 0x08303678,
    .regionalOrder = 0x0831DFB8,
    .regionalCount = 202,
    .regionName = "HOENN",
    .dexSeenCopy1 = 0x988,
    .dexSeenCopy2 = 0x3B24,
    .nationalMagicOff = 2,
    .nationalMagic = 0xDA,
    .trainers = 0x08310030,
    .learnsets = 0x0832937C,
    .wildHeaders = 0x08552D48,
    .flagsOff = 0x1270,
    .varsOff = 0x139C,
    .flagBytes = 0x12C,
    .probeTrainer = 265,
    .probeName = "ROXANNE",
    // TRAINER_CARD_EMERALD.
    .cardStyle = PD_CARD_HOENN,
    .gameStatsOff = 0x159C,
    .badgeFlag = 0x867,
    .pokedexFlag = 0x861,
    .nationalVar = 0x4046,
    .nationalVarValue = 0x302,
    .nationalFlag = 0x896,
    .frontierSymbolFlag = 0x8C4,
    .museumWinnersOff = 0x2E90 + 8 * 32,
    .frontierBpOff = 0xEBA,
};

// NATIVE_RUBY / NATIVE_SAPPHIRE (AXVE / AXPE, rev 1 and rev 2 - pret's
// pokeruby builds each byte-identical to retail; all four agree on every
// address below), with POKEDEX_RUBY / _SAPPHIRE and GUIDE_TABLES_RUBY /
// _SAPPHIRE. Emerald's engine with the switches pd_config describes: the
// party in IWRAM, static save blocks, no encryption key, gBagPockets in the
// ROM, inBattle at gMain+0x43D. No item icons (the R/S bag never draws one),
// no card; the battle controls but not the POKéMON switch (R/S's party menu
// keeps its cursor in a sprite, not gPartyMenu).
#define RUBY_SAPPHIRE_COMMON                                                                            \
    .playerParty = 0x03004360,                                                                          \
    .playerPartyCount = 0x03004350,                                                                     \
    .battleMons = 0x02024A80,                                                                           \
    .battlerPositions = 0x02024A72,                                                                     \
    .battlersCount = 0x02024A68,                                                                        \
    .battleTypeFlags = 0x020239F8,                                                                      \
    .gMain = 0x03001770,                                                                                \
    .saveBlock1Ptr = 0x02025734,                                                                        \
    .saveBlock2Ptr = 0x02024EA4,                                                                        \
    .staticSaveBlocks = true,                                                                           \
    .noEncryptionKey = true,                                                                            \
    .inBattleOff = 0x43D,                                                                               \
    .mapHeader = 0x0202E828,                                                                            \
    .moneyOff = 0x490,                                                                                  \
    .bagOrder = { PD_POCKET_ITEMS, PD_POCKET_BALLS, PD_POCKET_TMHM, PD_POCKET_BERRIES, PD_POCKET_KEY }, \
    .itemCount = 349,                                                                                   \
    .battlerControllerFuncs = 0x03004330,                                                               \
    .inputAction = 0x0802C098,                                                                          \
    .inputMove = 0x0802C68C,                                                                            \
    .inputTarget = 0x0802C2EC,                                                                          \
    .inputItem = 0x0802E03C,                                                                            \
    .inputParty = 0x0802DF88,                                                                           \
    .actionCursor = 0x02024E60,                                                                         \
    .moveCursor = 0x02024E64,                                                                           \
    /* FOE TEAM: gEnemyParty in IWRAM; monToSwitchIntoId at gSharedMem+0x16068. */                      \
    .enemyParty = 0x030045C0,                                                                           \
    .battlerPartyIndexes = 0x02024A6A,                                                                  \
    .battleStructPtr = 0x02000000,                                                                      \
    .battleStructStatic = true,                                                                         \
    .monToSwitchIntoOff = 0x16068,                                                                      \
    .dexEntryStride = 0x24,                                                                             \
    .dexPage2Off = 0x14,                                                                                \
    .regionalCount = 202,                                                                               \
    .regionName = "HOENN",                                                                              \
    .dexSeenCopy1 = 0x938,                                                                              \
    .dexSeenCopy2 = 0x3A8C,                                                                             \
    .nationalMagicOff = 2,                                                                              \
    .nationalMagic = 0xDA,                                                                              \
    .flagsOff = 0x1220,                                                                                 \
    .varsOff = 0x1340,                                                                                  \
    .flagBytes = 0x120,                                                                                 \
    .probeTrainer = 265,                                                                                \
    .probeName = "ROXANNE"

static const struct pd_config RUBY = {
    RUBY_SAPPHIRE_COMMON,
    .bagPockets = 0x083C1634,
    .items = 0x083C5580,
    .monIconTable = 0x083BBD3C,
    .monIconPaletteIndices = 0x083BC41C,
    .monIconPaletteTable = 0x083BC5D4,
    .dexEntries = 0x083B1874,
    .frontPics = 0x081E836C,
    .palettes = 0x081EA5CC,
    .speciesInfo = 0x081FEC30,
    .speciesToNational = 0x081FC52E,
    .abilityNames = 0x081FA260,
    .footprints = 0x083B4F00,
    .evolutions = 0x08203B80,
    .tmhmLearnsets = 0x081FD108,
    .tmhmMoves = 0x0837651C,
    .regionalOrder = 0x081FC864,
    .trainers = 0x081F0514,
    .learnsets = 0x08207BE0,
    .wildHeaders = 0x0839D46C,
    .mapGroups = 0x083085A0,
};

static const struct pd_config SAPPHIRE = {
    RUBY_SAPPHIRE_COMMON,
    .bagPockets = 0x083C1690,
    .items = 0x083C55DC,
    .monIconTable = 0x083BBD98,
    .monIconPaletteIndices = 0x083BC478,
    .monIconPaletteTable = 0x083BC630,
    .dexEntries = 0x083B18D0,
    .frontPics = 0x081E82FC,
    .palettes = 0x081EA55C,
    .speciesInfo = 0x081FEBC0,
    .speciesToNational = 0x081FC4BE,
    .abilityNames = 0x081FA1F0,
    .footprints = 0x083B4F5C,
    .evolutions = 0x08203B10,
    .tmhmLearnsets = 0x081FD098,
    .tmhmMoves = 0x083764AC,
    .regionalOrder = 0x081FC7F4,
    .trainers = 0x081F04A4,
    .learnsets = 0x08207B70,
    .wildHeaders = 0x0839D2B4,
    .mapGroups = 0x08308530,
};

// The CFRU hacks (NATIVE_UNBOUND_WITH_DEX / NATIVE_RADICAL_RED_V4_1): FireRed
// 1.0 patched with hooks, so its RAM - party, save blocks, gMain, the bag, the
// battle globals and the controller handlers - is FireRed rev 0's (the app
// checked it byte for byte against a clean rev 0 build); boxes are plaintext
// and the tables are the hacks' own, repointed. The battle menus' cursors are
// FireRed's (checked headless on both: unbound_battle / radical_red_battle).
#define CFRU_RAM                                \
    FIRERED_RAM,                                \
    .plainBoxes = true,                         \
    .battlerControllerFuncs = 0x03004FE0,       \
    .inputAction = 0x0802E438,                  \
    .inputMove = 0x0802EA10,                    \
    .inputTarget = 0x0802E674,                  \
    .inputItem = 0x0803073C,                    \
    .inputParty = 0x08030684,                   \
    .actionCursor = 0x02023FF8,                 \
    .moveCursor = 0x02023FFC

// Pokémon Unbound v2.1.1.1 (sha1 b4776b82...) and its French translation
// (0ce2a880..., the same code and RAM): CFRU repointed gItems; its party icons
// are UnboundIconSource's table, item icons sItemIconTable's.
static const struct pd_config UNBOUND = {
    CFRU_RAM,
    .items = 0x08876200,
    .monIconTable = 0x09A217FC,
    .monIconPaletteInline = true,
    .itemIcons = 0x0825A554,
};

// Pokémon Radical Red v4.1 (sha1 964f951a...): CFRU repointed the words retail
// rev 0 loads gMonIconTable / gMonIconPaletteIndices / sItemIconTable / gItems
// from; the palette table is still vanilla's.
static const struct pd_config RADICAL_RED = {
    CFRU_RAM,
    .items = 0x093C0000,
    .monIconTable = 0x097FE6CC,
    .monIconPaletteIndices = 0x097FE164,
    .monIconPaletteTable = 0x083D4038,
    .itemIcons = 0x093C8100,
};

struct known_hack {
    const char* sha1;
    enum pd_hack hack;
    const char* title;
    const struct pd_config* cfg;
    const struct pd_text* text;
};

// Poller.kt's *_SHA1 constants (keep in sync).
static const struct known_hack HACKS[] = {
    { "b4776b82a4c7915d0fadeaa27e013523f99dfd94", PD_HACK_UNBOUND, "Pokémon Unbound", &UNBOUND, &pd_text_unbound },
    // Its French translation: the companion shows Unbound's English names (the
    // app's French ones aren't ported); the ROM's own text (items, dex) is French.
    { "0ce2a880aa097f1dce4e1db8ee513d0e82d15859", PD_HACK_UNBOUND, "Pokémon Unbound (FR)", &UNBOUND, &pd_text_unbound },
    { "964f951a0fdaf209e4ea1344883ef0d557bb3a80", PD_HACK_RADICAL_RED, "Pokémon Radical Red", &RADICAL_RED, &pd_text_radical_red },
};

void pd_game_identify_sha1(struct pd_game* g, const char* sha1) {
    if (strcmp(g->code, "BPRE")) return;
    for (size_t i = 0; i < sizeof(HACKS) / sizeof(HACKS[0]); i++) {
        if (strcmp(sha1, HACKS[i].sha1)) continue;
        g->hack = HACKS[i].hack;
        g->title = HACKS[i].title;
        g->cfg = HACKS[i].cfg;
        g->unsupported = NULL;
        pd_text_use(HACKS[i].text);
        return;
    }
}

void pd_game_identify(struct pd_game* g) {
    // Only the hack-sized FireRed ROMs: a retail one is told by its header.
    if (strcmp(g->code, "BPRE") || g->romSize <= 0x1000000 || !g->rom) return;
    char sha1[41];
    pd_sha1_hex(g->rom, g->romSize, 0xC4, 6, sha1);
    pd_game_identify_sha1(g, sha1);
}

const uint8_t* pd_rom_at(const struct pd_game* g, uint32_t addr, size_t len) {
    if (!g->rom || addr < 0x08000000) return NULL;
    size_t off = addr - 0x08000000;
    if (off > g->romSize || len > g->romSize - off) return NULL;
    return g->rom + off;
}

// English rev 0 (RetailPortsGen's PORT_AXVE0 / PORT_AXPE0, sha1 f28b6ffc... /
// 3ccbbd45...): rev 1's RAM, its ROM tables a few bytes earlier.
static const struct pd_config RUBY_REV0 = {
    RUBY_SAPPHIRE_COMMON,
    .bagPockets = 0x083C1618,
    .items = 0x083C5564,
    .monIconTable = 0x083BBD20,
    .monIconPaletteIndices = 0x083BC400,
    .monIconPaletteTable = 0x083BC5B8,
    .dexEntries = 0x083B1858,
    .frontPics = 0x081E8354,
    .palettes = 0x081EA5B4,
    .speciesInfo = 0x081FEC18,
    .speciesToNational = 0x081FC516,
    .abilityNames = 0x081FA248,
    .footprints = 0x083B4EE4,
    .evolutions = 0x08203B68,
    .tmhmLearnsets = 0x081FD0F0,
    .tmhmMoves = 0x08376504,
    .regionalOrder = 0x081FC84C,
    .trainers = 0x081F04FC,
    .learnsets = 0x08207BC8,
    .wildHeaders = 0x0839D454,
    .mapGroups = 0x08308588,
};

static const struct pd_config SAPPHIRE_REV0 = {
    RUBY_SAPPHIRE_COMMON,
    .bagPockets = 0x083C1670,
    .items = 0x083C55BC,
    .monIconTable = 0x083BBD78,
    .monIconPaletteIndices = 0x083BC458,
    .monIconPaletteTable = 0x083BC610,
    .dexEntries = 0x083B18B0,
    .frontPics = 0x081E82E4,
    .palettes = 0x081EA544,
    .speciesInfo = 0x081FEBA8,
    .speciesToNational = 0x081FC4A6,
    .abilityNames = 0x081FA1D8,
    .footprints = 0x083B4F3C,
    .evolutions = 0x08203AF8,
    .tmhmLearnsets = 0x081FD080,
    .tmhmMoves = 0x08376494,
    .regionalOrder = 0x081FC7DC,
    .trainers = 0x081F048C,
    .learnsets = 0x08207B58,
    .wildHeaders = 0x0839D29C,
    .mapGroups = 0x08308518,
};

bool pd_game_is_rs(const struct pd_game* g) {
    return !strcmp(g->code, "AXVE") || !strcmp(g->code, "AXPE");
}

uint32_t pd_in_battle_addr(const struct pd_config* cfg) {
    return cfg->gMain + (cfg->inBattleOff ? cfg->inBattleOff : 0x439);
}

unsigned pd_rom_id(const struct pd_game* g) {
    return g->rom ? g->loadId : 0;
}

bool pd_rom_read(const struct pd_game* g, uint32_t addr, void* out, size_t len) {
    const uint8_t* p = pd_rom_at(g, addr, len);
    if (!p) return false;
    memcpy(out, p, len);
    return true;
}

void pd_game_detect(struct pd_game* g, const uint8_t* header, size_t romSize) {
    static unsigned loads;
    memset(g, 0, sizeof(*g));
    g->loadId = ++loads ? loads : ++loads;
    memcpy(g->code, header + 0xAC, 4);
    g->code[4] = 0;
    g->revision = header[0xBC];

    pd_text_use(&pd_text_firered);
    if (!strcmp(g->code, "BPRE") || !strcmp(g->code, "BPGE")) {
        g->kind = PD_GAME_FIRERED;
        g->title = g->code[2] == 'R' ? "Pokémon FireRed" : "Pokémon LeafGreen";
        bool rev1 = g->revision == 1;
        if (g->code[2] == 'R') g->cfg = rev1 ? &FIRERED_REV1 : &FIRERED_REV0_RAM_ONLY;
        else g->cfg = rev1 ? &LEAFGREEN_REV1 : &LEAFGREEN_REV0;
    } else if (!strcmp(g->code, "BPEE")) {
        g->kind = PD_GAME_EMERALD;
        g->title = "Pokémon Emerald";
        g->cfg = &EMERALD;
        pd_text_use(&pd_text_emerald);
    } else if (!strcmp(g->code, "AXVE") || !strcmp(g->code, "AXPE")) {
        g->kind = PD_GAME_EMERALD;
        bool ruby = g->code[1] == 'X' && g->code[2] == 'V';
        g->title = ruby ? "Pokémon Ruby" : "Pokémon Sapphire";
        pd_text_use(&pd_text_emerald);
        if (g->revision == 1 || g->revision == 2) {
            g->cfg = ruby ? &RUBY : &SAPPHIRE;
        } else if (g->revision == 0) {
            g->cfg = ruby ? &RUBY_REV0 : &SAPPHIRE_REV0;
        } else {
            g->unsupported = "This revision isn't supported yet";
            return;
        }
        if (romSize > 0x1000000) {
            g->cfg = NULL;
            g->title = ruby ? "Ruby ROM hack" : "Sapphire ROM hack";
            g->unsupported = "ROM hacks aren't supported on 3DS yet";
        }
        return;
    } else {
        g->title = "Unknown game";
        g->unsupported = "Not a game the 3DS companion knows yet";
        return;
    }
    if (g->revision > 1) {
        g->cfg = NULL;
        g->unsupported = "This revision isn't supported yet";
    } else if (romSize > 0x1000000) {
        // A >16 MB FireRed / Emerald is a ROM hack: the app tells them apart
        // by SHA1 (Poller.detect) - pd_game_identify, once the ROM is in.
        g->cfg = NULL;
        g->title = g->kind == PD_GAME_FIRERED ? "FireRed ROM hack" : "Emerald ROM hack";
        g->unsupported = "This ROM hack isn't supported on 3DS yet";
    }
}
