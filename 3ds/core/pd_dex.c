#include "pd_dex.h"

#include <stdio.h>
#include <string.h>

#include "pd_map.h"
#include "pd_snapshot.h"
#include "pd_tables.h"

#define SPECIES_COUNT 412

static uint16_t u16(const uint8_t* p) {
    return (uint16_t) (p[0] | p[1] << 8);
}

static uint32_t u32(const uint8_t* p) {
    return (uint32_t) p[0] | (uint32_t) p[1] << 8 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24;
}

// species -> national and back, built once per ROM.
static unsigned tablesRom;
static uint16_t toNational[SPECIES_COUNT];
static uint16_t toSpecies[PD_NATIONAL_COUNT + 1];
static bool available;

static void build(const struct pd_game* g) {
    if (tablesRom == pd_rom_id(g)) return;
    tablesRom = pd_rom_id(g);
    available = false;
    memset(toNational, 0, sizeof(toNational));
    memset(toSpecies, 0, sizeof(toSpecies));
    const struct pd_config* cfg = g->cfg;
    if (!cfg || !cfg->dexEntries || !g->rom) return;
    const uint8_t* nat = pd_rom_at(g, cfg->speciesToNational, (SPECIES_COUNT - 1) * 2);
    const uint8_t* e1 = pd_rom_at(g, cfg->dexEntries + cfg->dexEntryStride, cfg->dexEntryStride);
    if (!nat || !e1) return;
    char category[24];
    pd_gen3_text(e1, 12, category, sizeof(category));
    if (strcmp(category, "SEED") || u16(e1 + 0x0C) != 7 || u16(e1 + 0x0E) != 69 || u16(nat) != 1) return;
    for (int s = 1; s < SPECIES_COUNT; s++) {
        int n = u16(nat + (s - 1) * 2);
        toNational[s] = (uint16_t) n;
        // The lowest species id wins (the app's inverse).
        if (n >= 1 && n <= PD_NATIONAL_COUNT && !toSpecies[n]) toSpecies[n] = (uint16_t) s;
    }
    available = true;
}

bool pd_dex_available(const struct pd_game* g) {
    build(g);
    return available;
}

int pd_dex_species(const struct pd_game* g, int national) {
    build(g);
    return available && national >= 1 && national <= PD_NATIONAL_COUNT ? toSpecies[national] : 0;
}

int pd_dex_national(const struct pd_game* g, int species) {
    build(g);
    return available && species >= 1 && species < SPECIES_COUNT ? toNational[species] : 0;
}

int pd_dex_regional(const struct pd_game* g, int i) {
    const struct pd_config* cfg = g->cfg;
    if (!cfg || i < 1 || i > cfg->regionalCount) return 0;
    if (!cfg->regionalOrder) return i; // Kanto: national 1..151
    const uint8_t* p = pd_rom_at(g, cfg->regionalOrder + (uint32_t) (i - 1) * 2, 2);
    return p ? u16(p) : 0;
}

bool pd_dex_entry(const struct pd_game* g, int national, struct pd_dex_entry* out) {
    memset(out, 0, sizeof(*out));
    const struct pd_config* cfg = g->cfg;
    if (!pd_dex_available(g)) return false;
    const uint8_t* e = pd_rom_at(g, cfg->dexEntries + (uint32_t) national * cfg->dexEntryStride, cfg->dexEntryStride);
    if (!e) return false;
    out->national = national;
    out->species = pd_dex_species(g, national);
    pd_gen3_text(e, 12, out->category, sizeof(out->category));
    out->heightDm = u16(e + 0x0C);
    out->weightHg = u16(e + 0x0E);
    const uint8_t* d = pd_rom_at(g, u32(e + 0x10), 200);
    if (d) pd_gen3_text(d, 200, out->description, sizeof(out->description));
    // Ruby / Sapphire split the text over two pages: the second follows the first.
    const uint8_t* d2 = cfg->dexPage2Off ? pd_rom_at(g, u32(e + cfg->dexPage2Off), 200) : NULL;
    size_t n = strlen(out->description);
    if (d2 && n && n + 2 < sizeof(out->description)) {
        out->description[n++] = ' ';
        pd_gen3_text(d2, 200, out->description + n, sizeof(out->description) - n);
        if (!out->description[n]) out->description[n - 1] = 0;
    }

    const uint8_t* b = pd_rom_at(g, cfg->speciesInfo + (uint32_t) out->species * 28, 28);
    if (b) {
        // gBaseStats: HP ATK DEF SPEED SP.ATK SP.DEF; shown with SPEED last.
        static const int ORDER[6] = { 0, 1, 2, 4, 5, 3 };
        for (int i = 0; i < 6; i++) out->stats[i] = b[ORDER[i]];
        out->type1 = b[6];
        out->type2 = b[7];
        out->catchRate = b[8];
        out->genderRatio = b[0x10];
        out->eggGroup1 = b[0x14];
        out->eggGroup2 = b[0x15];
        int a1 = b[0x16], a2 = b[0x17];
        const uint8_t* n1 = a1 ? pd_rom_at(g, cfg->abilityNames + (uint32_t) a1 * 13, 13) : NULL;
        const uint8_t* n2 = a2 && a2 != a1 ? pd_rom_at(g, cfg->abilityNames + (uint32_t) a2 * 13, 13) : NULL;
        if (n1) pd_gen3_text(n1, 13, out->ability1, sizeof(out->ability1));
        if (n2) pd_gen3_text(n2, 13, out->ability2, sizeof(out->ability2));
    }
    return true;
}

bool pd_dex_sprite(const struct pd_game* g, int species, uint32_t* out) {
    const struct pd_config* cfg = g->cfg;
    if (!pd_dex_available(g) || species < 1 || species >= SPECIES_COUNT) return false;
    const uint8_t* pic = pd_rom_at(g, cfg->frontPics + (uint32_t) species * 8, 8);
    const uint8_t* pal = pd_rom_at(g, cfg->palettes + (uint32_t) species * 8, 8);
    if (!pic || !pal) return false;
    const uint8_t* picLz = pd_rom_at(g, u32(pic), 4);
    const uint8_t* palLz = pd_rom_at(g, u32(pal), 4);
    if (!picLz || !palLz) return false;
    // The decoded sizes come from the LZ77 headers: one 64x64 frame is 2048
    // bytes (Emerald's pics hold two frames; the first is used).
    size_t picSize = picLz[1] | (size_t) picLz[2] << 8 | (size_t) picLz[3] << 16;
    size_t palSize = palLz[1] | (size_t) palLz[2] << 8 | (size_t) palLz[3] << 16;
    if (picSize < 2048 || picSize > 8192 || palSize < 32 || palSize > 64) return false;
    static uint8_t tiles[8192], colours[64];
    size_t picAvail = g->romSize - (size_t) (picLz - g->rom), palAvail = g->romSize - (size_t) (palLz - g->rom);
    if (pd_lz77(picLz, picAvail, tiles, picSize) != picSize || pd_lz77(palLz, palAvail, colours, palSize) != palSize) {
        return false;
    }
    uint32_t rgb[16];
    for (int i = 0; i < 16; i++) {
        uint16_t c = (uint16_t) (colours[i * 2] | colours[i * 2 + 1] << 8);
        uint32_t r = c & 0x1F, gg = (c >> 5) & 0x1F, bb = (c >> 10) & 0x1F;
        rgb[i] = (r << 3 | r >> 2) << 16 | (gg << 3 | gg >> 2) << 8 | (bb << 3 | bb >> 2);
    }
    for (int y = 0; y < PD_SPRITE; y++) {
        for (int x = 0; x < PD_SPRITE; x++) {
            int tile = (y / 8) * 8 + x / 8;
            uint8_t v = tiles[tile * 32 + (y % 8) * 4 + (x % 8) / 2];
            int c = x & 1 ? v >> 4 : v & 0xF;
            out[y * PD_SPRITE + x] = c ? rgb[c] : 0xFF000000u;
        }
    }
    return true;
}

void pd_dex_height(int dm, char* buf, int len) {
    long inches = 10000L * dm / 254;
    if (inches % 10 >= 5) inches += 10;
    long feet = inches / 120, rest = (inches - feet * 120) / 10;
    snprintf(buf, (size_t) len, "%ld'%02ld\"", feet, rest);
}

void pd_dex_weight(int hg, char* buf, int len) {
    long lbs = 100000L * hg / 4536;
    if (lbs % 10 >= 5) lbs += 10;
    snprintf(buf, (size_t) len, "%ld.%ld lbs.", lbs / 100, (lbs % 100) / 10);
}

const char* pd_ability_name(const struct pd_game* g, int ability, char* buf, size_t len) {
    buf[0] = 0;
    const uint8_t* n = g->cfg && g->cfg->abilityNames && ability > 0
        ? pd_rom_at(g, g->cfg->abilityNames + (uint32_t) ability * 13, 13) : NULL;
    if (n) pd_gen3_text(n, 13, buf, len);
    return buf;
}

int pd_level_moves(const struct pd_game* g, int species, struct pd_level_move* out, int max) {
    // gLevelUpLearnsets[species] -> u16 level << 9 | move, ending 0xFFFF.
    const uint8_t* p = g->cfg && g->cfg->learnsets && species > 0 && species < SPECIES_COUNT
        ? pd_rom_at(g, g->cfg->learnsets + (uint32_t) species * 4, 4) : NULL;
    if (!p) return 0;
    uint32_t at = u32(p);
    int n = 0;
    for (; n < max; n++) {
        const uint8_t* e = pd_rom_at(g, at + (uint32_t) n * 2, 2);
        int v = e ? e[0] | e[1] << 8 : 0xFFFF;
        if (v == 0xFFFF || v == 0) break;
        out[n] = (struct pd_level_move) { v >> 9, v & 0x1FF };
    }
    return n;
}

#define TMHM_COUNT 58 // 50 TMs, 8 HMs

int pd_tm_moves(const struct pd_game* g, int species, struct pd_tm_move* out, int max) {
    const struct pd_config* cfg = g->cfg;
    if (!cfg || !cfg->tmhmLearnsets || !cfg->tmhmMoves || species < 1 || species >= SPECIES_COUNT) return -1;
    const uint8_t* bits = pd_rom_at(g, cfg->tmhmLearnsets + (uint32_t) species * 8, 8);
    const uint8_t* moves = pd_rom_at(g, cfg->tmhmMoves, TMHM_COUNT * 2);
    if (!bits || !moves) return -1;
    int n = 0;
    for (int i = 0; i < TMHM_COUNT && n < max; i++) {
        if (!(bits[i / 8] >> (i % 8) & 1)) continue;
        snprintf(out[n].label, sizeof(out[n].label), i < 50 ? "TM%02d" : "HM%02d", i < 50 ? i + 1 : i - 49);
        out[n].move = moves[i * 2] | moves[i * 2 + 1] << 8;
        n++;
    }
    return n;
}

const char* pd_egg_group_name(int group) {
    static const char* const NAMES[16] = {
        "", "MONSTER", "WATER 1", "BUG", "FLYING", "FIELD", "FAIRY", "GRASS", "HUMAN-LIKE",
        "WATER 3", "MINERAL", "AMORPHOUS", "WATER 2", "DITTO", "DRAGON", "UNDISCOVERED",
    };
    return group > 0 && group < 16 ? NAMES[group] : "";
}
