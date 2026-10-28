// The POKéDEX's ROM side (the app's Pokedex.kt): the species <-> national
// number tables, dex entries, base stats and the front sprite, read from the
// player's ROM; the save side (seen / caught) is in the snapshot.
#ifndef PD_DEX_H
#define PD_DEX_H

#include <stdbool.h>
#include <stdint.h>

#include "pd_game.h"

#define PD_SPRITE 64

struct pd_dex_entry {
    int national, species;
    char category[24]; // "SEED"
    int heightDm, weightHg;
    char description[320];
    int type1, type2;
    int stats[6]; // HP, ATK, DEF, SP.ATK, SP.DEF, SPEED (the app's order)
    char ability1[16], ability2[16];
    // gBaseStats' catch rate, gender ratio (PERCENT_FEMALE; 0 / 254 / 255 =
    // all male / female / genderless) and egg groups (EGG_GROUP_*).
    int catchRate, genderRatio, eggGroup1, eggGroup2;
};

// The DEX entry's MOVES (the app's DexDetails.levelUp / teachable).
struct pd_level_move {
    int level, move;
};
struct pd_tm_move {
    char label[8]; // "TM01", "HM05"
    int move;
};
int pd_level_moves(const struct pd_game* g, int species, struct pd_level_move* out, int max);
// TM / HM moves species can learn; -1 when the game's tables aren't known.
int pd_tm_moves(const struct pd_game* g, int species, struct pd_tm_move* out, int max);
// EGG_GROUP_* as the games' summaries name them ("WATER 1"), "" = none.
const char* pd_egg_group_name(int group);

// Whether g's ROM has this port's POKéDEX tables (the app's probe: entry 1
// is SEED, 7 dm, 69 hg; species 1 is national 1).
bool pd_dex_available(const struct pd_game* g);
// How many national numbers the game's dex has (386 retail; 0 = no dex).
int pd_dex_national_count(const struct pd_game* g);
// The species for a national number (0 = none), and back.
int pd_dex_species(const struct pd_game* g, int national);
int pd_dex_national(const struct pd_game* g, int species);
// The national number at regional position i (1-based); 0 = none.
int pd_dex_regional(const struct pd_game* g, int i);
bool pd_dex_entry(const struct pd_game* g, int national, struct pd_dex_entry* out);
// Ability id's name from the ROM's gAbilityNames (PokedexSource.abilityName);
// "" when the game's table isn't known.
const char* pd_ability_name(const struct pd_game* g, int ability, char* buf, size_t len);
// The front sprite, 64x64 0xRRGGBB with PD_MAP_CLEAR-style see-through
// (0xFF000000); false if it can't be decoded.
bool pd_dex_sprite(const struct pd_game* g, int species, uint32_t* out);
// "2'04\"" and "15.2 lbs." - the app's imperial conversions.
void pd_dex_height(int dm, char* buf, int len);
void pd_dex_weight(int hg, char* buf, int len);

#endif
