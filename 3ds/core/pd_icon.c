#include "pd_icon.h"

#include <string.h>

#include "pd_romart.h"
#include "pd_tables.h"

#define CLEAR 0xFF000000u
#define CACHE 24 // a party, a foe team and a few more

struct icon {
    unsigned rom;
    int species; // 0 = empty slot
    bool ok;
    uint32_t px[PD_ICON * PD_ICON];
};

static struct icon cache[CACHE];
static int nextSlot;

static uint32_t u32le(const uint8_t* p) {
    return (uint32_t) p[0] | (uint32_t) p[1] << 8 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24;
}

static uint32_t bgr555(uint16_t c) {
    uint32_t r = c & 0x1F, g = (c >> 5) & 0x1F, b = (c >> 10) & 0x1F;
    return (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
}

static bool decode(const struct pd_game* g, int species, uint32_t* out) {
    const struct pd_config* cfg = g->cfg;
    // Retail's ids end at the egg; a hack's at its own species table's end.
    int last = pd_game_text->speciesCount > PD_SPECIES_EGG ? pd_game_text->speciesCount - 1 : PD_SPECIES_EGG;
    if (!cfg || !cfg->monIconTable || species <= 0 || species > last) return false;
    uint8_t w[4], idx;
    if (!pd_rom_read(g, cfg->monIconTable + (uint32_t) species * 4, w, 4)) return false;
    const uint8_t* tiles = pd_rom_at(g, u32le(w), 16 * 32);
    const uint8_t* pal;
    if (cfg->monIconPaletteInline) {
        // Unbound: two frames of tiles (1024 bytes), then the icon's palette.
        pal = pd_rom_at(g, u32le(w) + 1024, 32);
        if (!tiles || !pal) return false;
    } else {
        if (!tiles || !pd_rom_read(g, cfg->monIconPaletteIndices + (uint32_t) species, &idx, 1) || idx > 15) return false;
        if (!pd_rom_read(g, cfg->monIconPaletteTable + (uint32_t) idx * 8, w, 4)) return false;
        pal = pd_rom_at(g, u32le(w), 32);
        if (!pal) return false;
    }
    uint32_t colours[16];
    for (int i = 0; i < 16; i++) colours[i] = bgr555((uint16_t) (pal[i * 2] | pal[i * 2 + 1] << 8));
    // 4x4 tiles of 8x8, row by row; a byte holds two pixels, the left one low.
    for (int t = 0; t < 16; t++) {
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                uint8_t b = tiles[t * 32 + y * 4 + x / 2];
                int v = x & 1 ? b >> 4 : b & 0x0F;
                out[((t / 4) * 8 + y) * PD_ICON + (t % 4) * 8 + x] = v ? colours[v] : CLEAR;
            }
        }
    }
    return true;
}

static const struct icon* icon_for(const struct pd_game* g, int species) {
    unsigned rom = pd_rom_id(g);
    for (int i = 0; i < CACHE; i++) {
        if (cache[i].species == species && cache[i].rom == rom) return &cache[i];
    }
    struct icon* e = &cache[nextSlot];
    nextSlot = (nextSlot + 1) % CACHE;
    e->rom = rom;
    e->species = species;
    e->ok = decode(g, species, e->px);
    return e;
}

bool pd_draw_mon_icon(struct pd_canvas* c, const struct pd_game* g, int species, int x, int y, bool faded) {
    if (!pd_rom_id(g)) return false;
    const struct icon* e = icon_for(g, species);
    if (!e->ok) return false;
    for (int py = 0; py < PD_ICON; py++) {
        int sy = y + py;
        if (sy < c->cy0 || sy >= c->cy1) continue;
        for (int px = 0; px < PD_ICON; px++) {
            int sx = x + px;
            uint32_t v = e->px[py * PD_ICON + px];
            if (v == CLEAR || sx < c->cx0 || sx >= c->cx1) continue;
            uint32_t* d = &c->px[sy * c->w + sx];
            if (faded) {
                uint32_t r = (((v >> 16) & 0xFF) * 35 + ((*d >> 16) & 0xFF) * 65) / 100;
                uint32_t gg = (((v >> 8) & 0xFF) * 35 + ((*d >> 8) & 0xFF) * 65) / 100;
                uint32_t b = ((v & 0xFF) * 35 + (*d & 0xFF) * 65) / 100;
                v = r << 16 | gg << 8 | b;
            }
            *d = v;
        }
    }
    return true;
}

// Item icons: one in the description bar at a time, a few cached.
#define ITEM_CACHE 4
struct item_icon {
    unsigned rom;
    int item;
    bool ok;
    uint32_t px[PD_ITEM_ICON * PD_ITEM_ICON];
};
static struct item_icon itemCache[ITEM_CACHE];
static int nextItemSlot;

static bool decode_item(const struct pd_game* g, int item, uint32_t* out) {
    const struct pd_config* cfg = g->cfg;
    uint8_t e[8];
    if (!cfg || !cfg->itemIcons || item <= 0 || !pd_rom_read(g, cfg->itemIcons + (uint32_t) item * 8, e, 8)) return false;
    const uint8_t* tilesLz = pd_rom_at(g, u32le(e), 16);
    const uint8_t* palLz = pd_rom_at(g, u32le(e + 4), 16);
    if (!tilesLz || !palLz) return false;
    size_t room = g->romSize - (u32le(e) - 0x08000000u);
    uint8_t tiles[9 * 32], pal[32];
    if (pd_lz77(tilesLz, room, tiles, sizeof(tiles)) < sizeof(tiles)) return false;
    room = g->romSize - (u32le(e + 4) - 0x08000000u);
    if (pd_lz77(palLz, room, pal, sizeof(pal)) < sizeof(pal)) return false;
    uint32_t colours[16];
    for (int i = 0; i < 16; i++) colours[i] = bgr555((uint16_t) (pal[i * 2] | pal[i * 2 + 1] << 8));
    for (int t = 0; t < 9; t++) {
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                uint8_t b = tiles[t * 32 + y * 4 + x / 2];
                int v = x & 1 ? b >> 4 : b & 0x0F;
                out[((t / 3) * 8 + y) * PD_ITEM_ICON + (t % 3) * 8 + x] = v ? colours[v] : CLEAR;
            }
        }
    }
    return true;
}

bool pd_draw_item_icon(struct pd_canvas* c, const struct pd_game* g, int item, int x, int y) {
    unsigned rom = pd_rom_id(g);
    if (!rom) return false;
    struct item_icon* e = NULL;
    for (int i = 0; i < ITEM_CACHE && !e; i++) {
        if (itemCache[i].item == item && itemCache[i].rom == rom) e = &itemCache[i];
    }
    if (!e) {
        e = &itemCache[nextItemSlot];
        nextItemSlot = (nextItemSlot + 1) % ITEM_CACHE;
        e->rom = rom;
        e->item = item;
        e->ok = decode_item(g, item, e->px);
    }
    if (!e->ok) return false;
    for (int py = 0; py < PD_ITEM_ICON; py++) {
        int sy = y + py;
        if (sy < c->cy0 || sy >= c->cy1) continue;
        for (int px = 0; px < PD_ITEM_ICON; px++) {
            int sx = x + px;
            uint32_t v = e->px[py * PD_ITEM_ICON + px];
            if (v != CLEAR && sx >= c->cx0 && sx < c->cx1) c->px[sy * c->w + sx] = v;
        }
    }
    return true;
}
