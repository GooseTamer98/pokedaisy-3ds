// The party menu's Pokémon icons, read from the player's ROM like the app's
// DecompIconSource: gMonIconTable[species] -> 4bpp tiles (32x64, two frames;
// the first is used), gMonIconPaletteIndices[species] -> one of
// gMonIconPaletteTable's 16-colour palettes. Nothing bundled.
#ifndef PD_ICON_H
#define PD_ICON_H

#include <stdbool.h>

#include "pd_canvas.h"
#include "pd_game.h"

#define PD_ICON 32

// Draws species' icon with its top-left at (x, y); faded = a fainted Pokémon
// (35% over what's behind it, like the app's FOE TEAM). False, drawing
// nothing, when the game's icon tables aren't known or don't decode.
bool pd_draw_mon_icon(struct pd_canvas* c, const struct pd_game* g, int species, int x, int y, bool faded);

#define PD_ITEM_ICON 24
// Draws item's bag icon (sItemIconTable: LZ77 tiles + palette, 24x24) with its
// top-left at (x, y); false, drawing nothing, when it doesn't decode.
bool pd_draw_item_icon(struct pd_canvas* c, const struct pd_game* g, int item, int x, int y);

#endif
