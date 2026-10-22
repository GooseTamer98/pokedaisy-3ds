// The ROM's evolution table (the app's Evolutions.kt / EvolutionSource):
// gEvolutionTable, five {method, param, target} rows per species - for the
// GUIDE's EVOLUTIONS page and the POKéDEX's EVOLVE section.
#ifndef PD_EVO_H
#define PD_EVO_H

#include <stdbool.h>
#include <stddef.h>

#include "pd_game.h"

#define PD_EVOS_PER_MON 5

struct pd_evo {
    int method; // EVO_* (include/constants/pokemon.h)
    int param;  // the level, the item, the beauty...
    int target; // the species it becomes
};

// Whether g's ROM has the table where its config says (BULBASAUR evolves at 16
// into species 2), like EvolutionSource.read's check.
bool pd_evo_available(const struct pd_game* g);
// species' evolutions (at most PD_EVOS_PER_MON); 0 when none or no table.
int pd_evolutions(const struct pd_game* g, int species, struct pd_evo* out);
// The species species evolves from (the first found), 0 = none.
int pd_evolves_from(const struct pd_game* g, int species, struct pd_evo* how);
// A plain level-up - the evolutions that need no explaining.
bool pd_evo_plain(const struct pd_evo* e);
// The spoiler-free half: how, not into what ("LEVEL UP", "USE AN ITEM" ...).
const char* pd_evo_kind(const struct pd_evo* e);
// The whole story: "LV 16 → IVYSAUR", "FIRE STONE → ARCANINE".
void pd_evo_text(const struct pd_game* g, const struct pd_evo* e, char* buf, size_t len);
// Just the how, without the target: "LV 16", "FIRE STONE", "Trade".
void pd_evo_how(const struct pd_game* g, const struct pd_evo* e, char* buf, size_t len);

#endif
