#include "pd_evo.h"

#include <stdio.h>
#include <string.h>

#include "pd_snapshot.h"
#include "pd_tables.h"

// include/constants/pokemon.h (identical in pokefirered and pokeemerald).
enum {
    EVO_FRIENDSHIP = 1,
    EVO_FRIENDSHIP_DAY,
    EVO_FRIENDSHIP_NIGHT,
    EVO_LEVEL,
    EVO_TRADE,
    EVO_TRADE_ITEM,
    EVO_ITEM,
    EVO_LEVEL_ATK_GT_DEF,
    EVO_LEVEL_ATK_EQ_DEF,
    EVO_LEVEL_ATK_LT_DEF,
    EVO_LEVEL_SILCOON,
    EVO_LEVEL_CASCOON,
    EVO_LEVEL_NINJASK,
    EVO_LEVEL_SHEDINJA,
    EVO_BEAUTY,
    EVO_MAX = EVO_BEAUTY,
};

#define EVO_STRIDE 8
#define SPECIES_COUNT 412

static int u16(const uint8_t* b) {
    return b[0] | b[1] << 8;
}

static int read_row(const struct pd_game* g, int species, int i, struct pd_evo* e) {
    const uint8_t* r = pd_rom_at(g, g->cfg->evolutions + (uint32_t) ((species * PD_EVOS_PER_MON + i) * EVO_STRIDE),
                                 EVO_STRIDE);
    if (!r) return 0;
    *e = (struct pd_evo) { u16(r), u16(r + 2), u16(r + 4) };
    return e->method >= 1 && e->method <= EVO_MAX && e->target >= 1 && e->target < SPECIES_COUNT;
}

bool pd_evo_available(const struct pd_game* g) {
    if (!g->cfg || !g->cfg->evolutions || !g->rom) return false;
    struct pd_evo e;
    return read_row(g, 1, 0, &e) && e.method == EVO_LEVEL && e.param == 16 && e.target == 2;
}

int pd_evolutions(const struct pd_game* g, int species, struct pd_evo* out) {
    if (species < 1 || species >= SPECIES_COUNT || !pd_evo_available(g)) return 0;
    int n = 0;
    for (int i = 0; i < PD_EVOS_PER_MON; i++) {
        if (read_row(g, species, i, &out[n])) n++;
    }
    return n;
}

int pd_evolves_from(const struct pd_game* g, int species, struct pd_evo* how) {
    if (!pd_evo_available(g)) return 0;
    struct pd_evo e;
    for (int s = 1; s < SPECIES_COUNT; s++) {
        for (int i = 0; i < PD_EVOS_PER_MON; i++) {
            if (read_row(g, s, i, &e) && e.target == species) {
                if (how) *how = e;
                return s;
            }
        }
    }
    return 0;
}

bool pd_evo_plain(const struct pd_evo* e) {
    return e->method == EVO_LEVEL;
}

const char* pd_evo_kind(const struct pd_evo* e) {
    switch (e->method) {
    case EVO_LEVEL: case EVO_LEVEL_NINJASK: return "LEVEL UP";
    case EVO_LEVEL_ATK_GT_DEF: case EVO_LEVEL_ATK_EQ_DEF: case EVO_LEVEL_ATK_LT_DEF: return "LEVEL UP (STATS)";
    case EVO_LEVEL_SILCOON: case EVO_LEVEL_CASCOON: return "LEVEL UP (RANDOM)";
    case EVO_LEVEL_SHEDINJA: return "LEVEL UP (BONUS)";
    case EVO_FRIENDSHIP: case EVO_FRIENDSHIP_DAY: case EVO_FRIENDSHIP_NIGHT: return "FRIENDSHIP";
    case EVO_TRADE: case EVO_TRADE_ITEM: return "TRADE";
    case EVO_ITEM: return "USE AN ITEM";
    case EVO_BEAUTY: return "BEAUTY";
    default: return "SPECIAL";
    }
}

void pd_evo_how(const struct pd_game* g, const struct pd_evo* e, char* buf, size_t len) {
    switch (e->method) {
    case EVO_LEVEL: case EVO_LEVEL_NINJASK: snprintf(buf, len, "LV %d", e->param); break;
    case EVO_LEVEL_ATK_GT_DEF: snprintf(buf, len, "LV %d, ATTACK > DEFENSE", e->param); break;
    case EVO_LEVEL_ATK_EQ_DEF: snprintf(buf, len, "LV %d, ATTACK = DEFENSE", e->param); break;
    case EVO_LEVEL_ATK_LT_DEF: snprintf(buf, len, "LV %d, ATTACK < DEFENSE", e->param); break;
    case EVO_LEVEL_SILCOON: case EVO_LEVEL_CASCOON:
        snprintf(buf, len, "LV %d (depends on the POKéMON)", e->param);
        break;
    case EVO_LEVEL_SHEDINJA: snprintf(buf, len, "LV %d, with a free party slot and a spare POKé BALL", e->param); break;
    case EVO_FRIENDSHIP: snprintf(buf, len, "Level up with high friendship"); break;
    case EVO_FRIENDSHIP_DAY: snprintf(buf, len, "Level up with high friendship, by day"); break;
    case EVO_FRIENDSHIP_NIGHT: snprintf(buf, len, "Level up with high friendship, at night"); break;
    case EVO_TRADE: snprintf(buf, len, "Trade"); break;
    case EVO_TRADE_ITEM: snprintf(buf, len, "Trade holding %s", pd_item_name(g, e->param)); break;
    case EVO_ITEM: snprintf(buf, len, "%s", pd_item_name(g, e->param)); break;
    case EVO_BEAUTY: snprintf(buf, len, "Level up with BEAUTY %d+", e->param); break;
    default: snprintf(buf, len, "?"); break;
    }
}

void pd_evo_text(const struct pd_game* g, const struct pd_evo* e, char* buf, size_t len) {
    char how[80], name[32];
    pd_evo_how(g, e, how, sizeof(how));
    snprintf(buf, len, "%s → %s", how, pd_species_name(e->target, name, sizeof(name)));
}
