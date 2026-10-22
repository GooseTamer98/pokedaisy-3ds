// The player's settings in a small text file (key=value lines), read at start
// and written on each change. Unknown keys and bad values fall back to the
// defaults, so an old or hand-edited file never breaks a start.
#ifndef PD_SETTINGS_H
#define PD_SETTINGS_H

#include <stdbool.h>

#include "pd_game.h"
#include "pd_ui.h"

void pd_settings_defaults(struct pd_settings* s);
// Missing file = defaults (and false).
bool pd_settings_load(struct pd_settings* s, const char* path);
bool pd_settings_save(const struct pd_settings* s, const char* path);

// g's starred items; NULL with create false when it has none yet. Creating
// reuses the oldest slot once all PD_FAV_GAMES are taken.
struct pd_favorites* pd_favorites_for(struct pd_settings* s, const struct pd_game* g, bool create);
bool pd_favorite(const struct pd_favorites* f, int item);
// Stars or unstars item (the end of the list when starred).
void pd_favorite_toggle(struct pd_settings* s, const struct pd_game* g, int item);

#endif
