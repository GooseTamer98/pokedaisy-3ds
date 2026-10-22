// The BATTLE tab's controls driving the game's own battle menu - the app's
// BattleInputController for the Gen 3 menus: button presses a player could
// make, nothing patched. Where the game keeps its menu cursors (pd_config's
// actionCursor / moveCursor) a pick writes the cell there and presses A once
// (one select sound, as in the app); a switch steers the game's party menu by
// gPartyMenu.slotId to the Pokémon (found by personality - the game reorders
// the party in battle), then A, A on SHIFT; RUN presses B through "Got away
// safely!". While a pick presses or steers, the player's own keys wait; the
// passive watches after it (a target screen, RUN's messages) never lock them
// out, and a watchdog ends anything that runs too long.
#ifndef PD_BCTL_H
#define PD_BCTL_H

#include <stdbool.h>
#include <stdint.h>

#include "pd_action.h"
#include "pd_game.h"
#include "pd_snapshot.h"

// Which battle menu waits for the player (the app's BATTLE_INPUT_*).
enum pd_bin {
    PD_BIN_NONE,   // not in a battle
    PD_BIN_BUSY,   // the battle is playing out
    PD_BIN_ACTION, // FIGHT / BAG / POKéMON / RUN
    PD_BIN_MOVE,
    PD_BIN_BAG,
    PD_BIN_PARTY,
    PD_BIN_TARGET,
};

// The state now, and whose menu it is (battler index); PD_BIN_NONE with no
// battle input for this game.
enum pd_bin pd_battle_input(const struct pd_game* g, pd_read_fn read, void* ctx, int* battler);
bool pd_battle_input_known(const struct pd_game* g);

enum pd_bc_goal { PD_BC_NONE, PD_BC_ACTION, PD_BC_MOVE, PD_BC_SWITCH };

struct pd_bctl {
    enum pd_bc_goal goal;
    int cell; // the action (0 FIGHT, 1 BAG, 2 POKéMON, 3 RUN) or the move
    uint32_t personality; // SWITCH's Pokémon
    int stage, frames, waited, presses;
    int hold, gap;
    uint16_t keys;
    int targetWatch, runWatch, runGap;
    enum pd_bin state; // as last read
    int battler;
};

void pd_bctl_action(struct pd_bctl* b, int action);
void pd_bctl_move(struct pd_bctl* b, int move);
void pd_bctl_switch(struct pd_bctl* b, uint32_t personality);
// BACK on the game's bag / party screen (B).
void pd_bctl_back(struct pd_bctl* b);
bool pd_bctl_busy(const struct pd_bctl* b);
// Before each frame: the keys the game gets - the player's, or the pick's.
uint16_t pd_bctl_frame(struct pd_bctl* b, const struct pd_game* g, const struct pd_game_io* io, uint16_t player);

#endif
