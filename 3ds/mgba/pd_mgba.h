// The companion's way into a running mGBA core (pd_action.h's pd_game_io):
// reads, RAM writes, and calls to the game's own functions between frames -
// the app's pk_call (app/src/main/cpp/pokedaisy_jni.c), keep in sync. Shared
// by the 3DS host (ctr/main.c) and the desktop test (desktop/pd_act_test.c).
#ifndef PD_MGBA_H
#define PD_MGBA_H

#include <stdbool.h>
#include <stdint.h>

#include "pd_action.h"

struct mCore;

// After every runFrame: learns the main loop's resting stack depth, which
// tells a callable moment from one mid-way through the game's own code.
void pd_mgba_frame_done(struct mCore* core);
// Forgets it (a new game, a loaded state).
void pd_mgba_reset(void);
// io for core.
void pd_mgba_io(struct pd_game_io* io, struct mCore* core);

#endif
