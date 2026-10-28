// Name / move / type tables, generated from the app's Kotlin tables by
// 3ds/tools/gen_tables.py (pd_tables_gen.c), plus small lookups over them.
#ifndef PD_TABLES_H
#define PD_TABLES_H

#include <stdbool.h>
#include <stddef.h>

struct pd_move_info {
    const char* name;
    int type;
    int power;
};

struct pd_type_pair {
    int atk, def, pct;
};

extern const char* const pd_species_names[];
extern const int pd_species_names_count;
extern const struct pd_move_info pd_moves[];
extern const int pd_moves_count;
extern const char* const pd_type_names[];
extern const int pd_type_names_count;
extern const struct pd_type_pair pd_type_chart[];
extern const int pd_type_chart_count;
extern const char* const pd_mapsec_firered[];
extern const int pd_mapsec_firered_count;
extern const char* const pd_mapsec_emerald[];
extern const int pd_mapsec_emerald_count;
extern const char* const pd_item_names_firered[];
extern const int pd_item_names_firered_count;
extern const char* const pd_item_names_emerald[];
extern const int pd_item_names_emerald_count;
// gBaseStats[species].genderRatio (FireRed's numbering, which Emerald shares).
extern const unsigned char pd_gender_ratios[];
extern const int pd_gender_ratios_count;
extern const unsigned char pd_species_type_table[][2];
extern const int pd_species_type_table_count;
// The summary's EXP bar (ExpTables.kt): each species' growth rate (GROWTH_*,
// 0..5) and gExperienceTables[rate][level], the total EXP that level starts at.
extern const unsigned char pd_growth_rates[];
extern const int pd_growth_rates_count;
extern const int pd_exp_tables[6][101];
// The move advice's data (the app's MoveStats, from gBattleMoves): the
// effect (EFFECT_* without the prefix), accuracy (0 = never misses), PP, the
// effect's chance and priority, by move id.
struct pd_move_stats {
    const char* effect;
    int accuracy, pp, chance, priority;
};
extern const struct pd_move_stats pd_move_stats[];
extern const int pd_move_stats_count;
// The type chips' colours (AppColors.typeColors), 0xRRGGBB by type id
// (the CFRU hacks' Fairy is 23).
extern const unsigned int pd_type_colors[];
extern const int pd_type_colors_count;

// One game's names and tables (the app's ActiveTables): FireRed and Emerald
// share the Gen 3 ones (their own items and places); the hacks have their own
// ids. NULL growth / move stats = no EXP bar / move advice for that game.
struct pd_text {
    const char* const* species;
    int speciesCount;
    const struct pd_move_info* moves;
    int movesCount;
    const char* const* items;
    int itemsCount;
    const unsigned char (*speciesTypes)[2];
    int speciesTypesCount;
    const unsigned char* genderRatios;
    int genderRatiosCount;
    const char* const* typeNames;
    int typeNamesCount;
    const struct pd_type_pair* chart;
    int chartCount;
    const char* const* mapsecs;
    int mapsecsCount;
    const unsigned char* growthRates;
    int growthRatesCount;
    const struct pd_move_stats* moveStats;
    int moveStatsCount;
    bool ownCase; // names already in the game's casing (the hacks' mixed case)
};
extern const struct pd_text pd_text_firered, pd_text_emerald, pd_text_unbound, pd_text_radical_red;
// The running game's set, picked by pd_game_detect / pd_game_identify (FireRed's until then).
extern const struct pd_text* pd_game_text;
void pd_text_use(const struct pd_text* t);

#define PD_TYPE_NONE 255
#define PD_SPECIES_EGG 412

// Names in the game's own casing: FireRed / Emerald print species and moves
// in capitals with a small é (the app's gameCase()). Writes into buf.
const char* pd_species_name(int species, char* buf, size_t len);
const char* pd_move_name(int move, char* buf, size_t len);
const char* pd_type_name(int type);
const char* pd_item_text(int item);    // NULL when unnamed
const char* pd_mapsec_text(int mapsec); // NULL when unnamed
int pd_type_count(void);               // type ids run 0..this-1 (some unnamed)
const struct pd_move_stats* pd_move_stats_of(int move); // NULL without move advice
int pd_growth_rate(int species);       // -1 when unknown
int pd_gender_ratio(int species);      // -1 when unknown
int pd_move_type(int move);
int pd_move_power(int move);

// One attacking type against a (possibly dual-typed) defender, in percent
// (100 = 1x) - the app's typeMultiplierPct().
int pd_type_multiplier_pct(int atk, int def1, int def2);
// A species' types (the app's speciesTypeData; the same type twice for one),
// PD_TYPE_NONE both when unknown.
void pd_species_type_pair(int species, int* t1, int* t2);

#endif
