// Which game is running and where its RAM globals live - the C twin of the
// app's NativeConfig (app/.../companion/data/NativeReader.kt). Only the
// fields this port reads so far; the addresses are copied from there (each
// comes from a pret decomp build byte-identical to retail), keep in sync.
#ifndef PD_GAME_H
#define PD_GAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum pd_game_kind {
    PD_GAME_NONE = 0,
    PD_GAME_FIRERED,  // FireRed and LeafGreen (LeafGreen runs as FireRed, like the app)
    PD_GAME_EMERALD,  // Emerald, Ruby and Sapphire (R/S run as Emerald, like the app)
};

// A ROM hack the 3DS companion knows by its SHA-1 (the app's GameKind past
// the retail ones). It runs on its base game's engine (pd_game.kind).
enum pd_hack {
    PD_HACK_NONE = 0,
    PD_HACK_UNBOUND,     // Pokémon Unbound v2.1.1.1 (English; its French translation too)
    PD_HACK_RADICAL_RED, // Pokémon Radical Red v4.1
};

struct pd_config {
    uint32_t playerParty;
    uint32_t playerPartyCount;
    uint32_t battleMons;
    uint32_t battlerPositions;
    uint32_t battlersCount;
    uint32_t battleTypeFlags;
    uint32_t gMain;
    uint32_t saveBlock1Ptr;
    uint32_t saveBlock2Ptr;
    uint32_t mapHeader;
    uint32_t encryptionKeyOff; // in SaveBlock2
    uint32_t moneyOff;         // in SaveBlock1
    // Ruby / Sapphire's engine (the app's NATIVE_RUBY): saveBlock1Ptr / 2Ptr
    // are the save blocks themselves (no pointers, no per-load shuffle), no
    // encryption key (money and bag quantities are plain), gBagPockets is a
    // ROM table, gMain.inBattle sits at +0x43D (0 = Emerald / FireRed's
    // 0x439) and gBattleStruct is static (battleStructPtr = the struct).
    bool staticSaveBlocks, noEncryptionKey, battleStructStatic;
    uint32_t inBattleOff;
    // The battle's FOE TEAM: gEnemyParty (6 struct Pokemon), gBattlerPartyIndexes
    // (u16 per battler: which one is out) and gBattleStruct (a pointer) ->
    // monToSwitchIntoId[battler] (the trainer's next pick). 0 = unknown.
    uint32_t enemyParty;
    uint32_t battlerPartyIndexes;
    uint32_t battleStructPtr;
    uint32_t monToSwitchIntoOff;
    // The party menu's icons in the ROM: gMonIconTable (a tiles pointer per
    // species), gMonIconPaletteIndices (u8) and gMonIconPaletteTable
    // ({data, tag}, 8 bytes). 0 = none.
    uint32_t monIconTable, monIconPaletteIndices, monIconPaletteTable;
    // Unbound's gMonIconTable points at 1024 bytes of tiles with their own
    // 32-byte palette right after (UnboundIconSource), no palette tables.
    bool monIconPaletteInline;
    // CFRU's boxes are plaintext: a party slot whose checksum fails is read
    // unencrypted, in growth / attacks / EVs / misc order (decodePartyMon's fallback).
    bool plainBoxes;
    // gBagPockets: five {slots pointer, capacity} pairs, 8 bytes apart;
    // bagOrder[i] is the pocket (PD_POCKET_*) the game keeps at index i.
    uint32_t bagPockets;
    uint8_t bagOrder[5];
    // gItems in the ROM (the app's vanillaItems: 44-byte struct Item, itemId
    // +0x0E, description pointer +0x14) - the bag's descriptions are read from
    // the player's ROM, never bundled. 0 = none.
    uint32_t items;
    int itemCount; // ITEMS_COUNT where ids past it read past gItems (R/S: 349); 0 = no limit
    // sItemIconTable: {LZ77 tiles, LZ77 palette} per item, 24x24 (DecompIconSource.fetchItem).
    uint32_t itemIcons;

    // Battle input (the app's NativeConfig / BattleInputController):
    // gBattlerControllerFuncs (a function per battler) and the player's
    // controller handlers it holds while a menu waits - the action menu, the
    // moves, the target, the bag, the party; gActionSelectionCursor /
    // gMoveSelectionCursor (a cell per battler) and gPartyMenu (slotId +9).
    // 0 = no battle input.
    uint32_t battlerControllerFuncs;
    uint32_t inputAction, inputMove, inputTarget, inputItem, inputParty;
    uint32_t actionCursor, moveCursor, partyMenu;

    // POKéDEX (the app's PokedexTables): ROM tables ...
    uint32_t dexEntries;       // gPokedexEntries, by national number
    uint32_t dexEntryStride;
    uint32_t dexPage2Off;      // R/S: a second description pointer (page 2); 0 = one page
    uint32_t speciesInfo;      // gBaseStats, 28 bytes per species
    uint32_t speciesToNational; // u16 per species from species 1
    uint32_t abilityNames;     // 13 bytes each
    uint32_t frontPics;        // {LZ77 pointer, size, tag} per species
    uint32_t palettes;         // {LZ77 pointer, tag} per species
    uint32_t regionalOrder;    // u16 national numbers; 0 = national 1..regionalCount
    // The DEX entry's sections and the GUIDE's EVOLUTIONS (PokedexTables /
    // GuideTables): gEvolutionTable (5 x {method, param, target} per species),
    // the TM / HM compatibility (8 bytes a species) and sTMHMMoves (u16 per
    // TM / HM), gMonFootprintTable (a pointer per species) and gMapGroups.
    uint32_t evolutions, tmhmLearnsets, tmhmMoves, footprints, mapGroups;
    int regionalCount;
    const char* regionName;
    // The hacks' dex (POKEDEX_UNBOUND / _RADICAL_RED): more species and
    // national numbers (0 = retail's 412 / 386), 17-byte ability names,
    // regionalOrder listing species ids, the National Dex always there.
    int dexSpeciesCount, dexNationalCount, abilityNameLen;
    bool regionalOrderIsSpecies, nationalAlways;
    // CFRU keeps seen / caught in SaveBlock1 (no anti-cheat copies): offsets
    // and bytes; 0 = struct Pokedex in SaveBlock2 with dexSeenCopy1 / 2.
    uint32_t dexSeenSb1, dexCaughtSb1;
    int dexFlagBytes;
    const char* dexProbe; // entry 1's category (checks the tables); NULL = "SEED"
    // ... and the save: SaveBlock2's owned / seen flags, SaveBlock1's two
    // copies of seen (all must agree), the National Dex byte.
    uint32_t dexSeenCopy1, dexSeenCopy2; // in SaveBlock1
    uint8_t nationalMagicOff, nationalMagic; // SaveBlock2 + 0x18 + off == magic

    // GUIDE (the app's GuideTables): ROM tables and SaveBlock1 offsets.
    uint32_t trainers;    // gTrainers, 0x28 bytes each
    uint32_t learnsets;   // gLevelUpLearnsets, a pointer per species
    uint32_t wildHeaders; // gWildMonHeaders, 20 bytes each
    uint32_t flagsOff, varsOff, flagBytes; // in SaveBlock1
    int probeTrainer;     // checks the tables match the ROM ...
    const char* probeName; // ... by this trainer's name
    bool learnsetCfru;    // CFRU's learnsets: {u16 move, u8 level}, ending at level 0xFF
    uint32_t mapGroupsXor; // gMapGroups' group pointers XOR this (Unbound's hooked lookup)

    // TRAINER CARD (the app's TrainerCardSave): which card (PD_CARD_*, 0 =
    // none), SaveBlock1.gameStats, the badge / POKéDEX flags, what makes the
    // National Dex count (with nationalMagic above), FireRed's sticker vars and
    // Emerald's extra stars (Battle Frontier symbols, the museum's paintings)
    // and battle points. 0 = not in this game.
    int cardStyle;
    uint32_t gameStatsOff;
    int badgeFlag, pokedexFlag;
    int nationalVar, nationalVarValue, nationalFlag;
    int stickerVar;
    int frontierSymbolFlag;
    uint32_t museumWinnersOff, frontierBpOff; // in SaveBlock1 / SaveBlock2
};

enum pd_card_style { PD_CARD_NONE, PD_CARD_KANTO, PD_CARD_HOENN };

// The bag's pockets, in a fixed order of our own (the app's POCKET_* ids).
enum pd_pocket {
    PD_POCKET_ITEMS,
    PD_POCKET_BALLS,
    PD_POCKET_TMHM,
    PD_POCKET_BERRIES,
    PD_POCKET_KEY,
    PD_POCKET_COUNT,
};

struct pd_game {
    enum pd_game_kind kind;
    char code[5];       // ROM header game code, e.g. "BPRE"
    int revision;       // ROM header 0xBC
    const char* title;  // "Pokémon FireRed"
    enum pd_hack hack;
    const struct pd_config* cfg; // NULL = detected but not supported
    const char* unsupported;     // why, when cfg is NULL
    // The ROM's bytes (0x08000000 on the bus), for the tabs that read its
    // tables and art; the host sets them once the ROM is loaded (NULL = none).
    const uint8_t* rom;
    size_t romSize;
    unsigned loadId; // new on every pd_game_detect (see pd_rom_id)
};

// Which ROM the tabs' caches were built from: 0 = none, else new for every
// game loaded. Not the ROM pointer - mGBA's 3DS build loads every game into
// the same fixed buffer.
unsigned pd_rom_id(const struct pd_game* g);

// Reads from the ROM at a bus address (0x08xxxxxx); false if out of range.
bool pd_rom_read(const struct pd_game* g, uint32_t addr, void* out, size_t len);
// A pointer into the ROM at a bus address, with at least len bytes after it.
const uint8_t* pd_rom_at(const struct pd_game* g, uint32_t addr, size_t len);

// Ruby / Sapphire (AXVE / AXPE), which run as PD_GAME_EMERALD.
bool pd_game_is_rs(const struct pd_game* g);
// gMain.inBattle's byte.
uint32_t pd_in_battle_addr(const struct pd_config* cfg);

// Reads the ROM header (the first 0xC0 bytes of the ROM) and its size.
void pd_game_detect(struct pd_game* g, const uint8_t* header, size_t romSize);
// Once g->rom is set: a ROM too big for retail is told apart by its SHA-1
// (whole file, the cart's GPIO bytes 0xC4-0xC9 read as zeros, as the app's).
// A second or so over 32 MB on a New 3DS, once per game loaded.
void pd_game_identify(struct pd_game* g);
// The same with the hash given (the preview's fixtures have no ROM).
void pd_game_identify_sha1(struct pd_game* g, const char* sha1);

#endif
