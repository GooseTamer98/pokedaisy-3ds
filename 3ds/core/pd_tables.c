#include "pd_tables.h"

#include <stdio.h>
#include <string.h>

const struct pd_text* pd_game_text = &pd_text_firered;

void pd_text_use(const struct pd_text* t) {
    pd_game_text = t ? t : &pd_text_firered;
}

// Upper-cases ASCII letters, leaving é (UTF-8 C3 A9) and the rest alone - the
// app's gameCase() for FireRed / Emerald; a hack's names are printed as they are.
static const char* game_case(const char* src, char* buf, size_t len) {
    if (pd_game_text->ownCase) {
        snprintf(buf, len, "%s", src);
        return buf;
    }
    size_t i = 0;
    for (; src[i] && i + 1 < len; i++) {
        char c = src[i];
        buf[i] = (c >= 'a' && c <= 'z') ? (char) (c - 'a' + 'A') : c;
    }
    buf[i] = 0;
    return buf;
}

const char* pd_species_name(int species, char* buf, size_t len) {
    if (species == 0) return "-";
    if (species == PD_SPECIES_EGG) return pd_game_text->ownCase ? "Egg" : "EGG";
    if (species > 0 && species < pd_game_text->speciesCount && pd_game_text->species[species]) {
        return game_case(pd_game_text->species[species], buf, len);
    }
    snprintf(buf, len, "#%d", species);
    return buf;
}

const char* pd_move_name(int move, char* buf, size_t len) {
    if (move <= 0) return "-";
    if (move < pd_game_text->movesCount && pd_game_text->moves[move].name) return game_case(pd_game_text->moves[move].name, buf, len);
    snprintf(buf, len, "MOVE#%d", move);
    return buf;
}

const char* pd_type_name(int type) {
    if (type >= 0 && type < pd_game_text->typeNamesCount && pd_game_text->typeNames[type]) return pd_game_text->typeNames[type];
    return "";
}

int pd_type_count(void) {
    return pd_game_text->typeNamesCount;
}

const char* pd_item_text(int item) {
    return item > 0 && item < pd_game_text->itemsCount ? pd_game_text->items[item] : NULL;
}

const char* pd_mapsec_text(int mapsec) {
    return mapsec >= 0 && mapsec < pd_game_text->mapsecsCount ? pd_game_text->mapsecs[mapsec] : NULL;
}

const struct pd_move_stats* pd_move_stats_of(int move) {
    return pd_game_text->moveStats && move > 0 && move < pd_game_text->moveStatsCount ? &pd_game_text->moveStats[move] : NULL;
}

int pd_growth_rate(int species) {
    return pd_game_text->growthRates && species > 0 && species < pd_game_text->growthRatesCount ? pd_game_text->growthRates[species] : -1;
}

int pd_gender_ratio(int species) {
    return species >= 0 && species < pd_game_text->genderRatiosCount ? pd_game_text->genderRatios[species] : -1;
}

int pd_move_type(int move) {
    if (move > 0 && move < pd_game_text->movesCount && pd_game_text->moves[move].name) return pd_game_text->moves[move].type;
    return PD_TYPE_NONE;
}

int pd_move_power(int move) {
    if (move > 0 && move < pd_game_text->movesCount && pd_game_text->moves[move].name) return pd_game_text->moves[move].power;
    return 0;
}

static int single_pct(int atk, int def) {
    for (int i = 0; i < pd_game_text->chartCount; i++) {
        if (pd_game_text->chart[i].atk == atk && pd_game_text->chart[i].def == def) return pd_game_text->chart[i].pct;
    }
    return 100;
}

int pd_type_multiplier_pct(int atk, int def1, int def2) {
    int pct = single_pct(atk, def1);
    if (def2 != PD_TYPE_NONE && def2 != def1) pct = pct * single_pct(atk, def2) / 100;
    return pct;
}

void pd_species_type_pair(int species, int* t1, int* t2) {
    *t1 = *t2 = PD_TYPE_NONE;
    if (species <= 0 || species >= pd_game_text->speciesTypesCount) return;
    *t1 = pd_game_text->speciesTypes[species][0];
    *t2 = pd_game_text->speciesTypes[species][1];
}
