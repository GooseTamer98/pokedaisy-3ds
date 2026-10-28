// Shared by the companion's tab files (pd_ui_*.c): layout, hit ids and the
// small widgets every tab draws with.
#ifndef PD_UI_INTERNAL_H
#define PD_UI_INTERNAL_H

#include <stddef.h>

#include "pd_style.h"
#include "pd_ui.h"

#define CONTENT_Y 28
#define MARGIN 4
#define ROW_H 18

// Hit ids.
enum {
    HIT_TAB = 100,         // + tab
    HIT_FF = 110,
    HIT_SLOT = 200,        // + party slot
    HIT_BACK = 300,
    HIT_POCKET_PREV = 400,
    HIT_POCKET_NEXT = 401,
    HIT_BAG_SORT = 402,
    HIT_BAG_STAR = 403,
    HIT_BAG_LIST = 410,
    HIT_SETTING = 600,     // + enum setting_row
    HIT_OPTION = 700,      // + pick-list option
    HIT_YES = 800,
    HIT_NO = 801,
    HIT_SCRIM = 802,
    HIT_MAP = 900,         // the map itself (where: the touch's start)
    HIT_MAP_REGION = 901,
    HIT_MAP_ME = 902,
    HIT_MAP_PLACES = 903,
    HIT_PLACE = 1000,      // + PLACES row
    HIT_DEX_LIST = 1400,
    HIT_DEX_ROW = 5000,    // + national number (up to PD_NATIONAL_MAX)
    HIT_DEX_TOGGLE = 1900,
    HIT_DEX_PREV = 1901,
    HIT_DEX_NEXT = 1902,
    HIT_DEX_SECTION = 1910, // + PD_DEX_*
    HIT_DEX_SECTION_LIST = 1920,
    HIT_GUIDE_PAGE = 2000, // + page
    HIT_GUIDE_ROW = 2100,  // + entry
    HIT_GUIDE_LIST = 2900,
    HIT_NOTICE_OK = 2901,
    HIT_NOTICE_BACK = 2902,
    HIT_BATTLE_SUGGEST = 3000,
    HIT_BATTLE_INFO = 3001,
    HIT_BATTLE_FOE = 3002, // the foe's card: FOE DETAILS
    HIT_FOE_SLOT = 3010,   // + foe party index
    HIT_OPEN_CARD = 3100,
    HIT_CARD = 3101,
    HIT_SUMMARY_PAGE = 3200, // + PD_SUMMARY_*
    HIT_LEARN_LIST = 3290,
    HIT_LEARN_ROW = 3300,    // + LEARN option
    HIT_BAG_ROW = 4000,      // + row (ALL lists every pocket: up to 320)
    HIT_ACT_USE = 3400,
    HIT_ACT_GIVE = 3401,
    HIT_ACT_TAKE = 3402,
    HIT_ACT_PC = 3403,
    HIT_ACT_FLY = 3404,
    HIT_ACT_OK = 3405,
    HIT_ACT_SLOT = 3410,     // + party slot
    HIT_ACT_MOVE = 3420,     // + move slot
    HIT_FLY_LIST = 3499,
    HIT_BC_FIGHT = 3600,     // + action (FIGHT, BAG, POKéMON, RUN)
    HIT_BC_CANCEL = 3604,
    HIT_BC_BACK = 3605,      // B on the game's bag / party screen
    HIT_BC_CONTROLS = 3606,  // the bar's way back to CONTROLS
    HIT_BC_MOVE = 3610,      // + move slot
    HIT_BC_SLOT = 3620,      // + party slot
    HIT_FLY_TOWN = 3500,     // + town (PD_FLY_MAX)
};

enum setting_row {
    SETTING_SCREEN,
    SETTING_FF,
    SETTING_SAVE_STATE,
    SETTING_LOAD_STATE,
    SETTING_LEAVE,
};

void ui_add_hit(struct pd_ui* ui, int x, int y, int w, int h, int id);
void ui_add_scroll_hit(struct pd_ui* ui, int x, int y, int w, int h, int id);

// A white framed button; pressed = grey, like the tab chips.
void ui_button(struct pd_ui* ui, struct pd_canvas* c, int x, int y, int w, int h, const char* label, int id);
// The party menu's HP bar: a dark frame, the "HP" tag, a two-tone fill.
void ui_hp_bar(struct pd_canvas* c, int x, int y, int barW, int hp, int maxHp);
void ui_status_badge(struct pd_canvas* c, int x, int y, uint32_t status);
// A message in a list window across the content area.
void ui_notice(struct pd_canvas* c, const char* title, const char* line1, const char* line2);
// A LABEL  VALUE row, the value from ~42% across (OptionLine).
void ui_row(struct pd_canvas* c, int x, int y, int w, const char* label, const char* value, bool selected);
void ui_format_money(long v, char* buf, size_t len);
// A type chip in the type's colour (TypeBadge), with " suffix" if given (a
// matchup's "2x"); returns its width, 0 for no type.
int ui_type_chip(struct pd_canvas* c, int x, int y, int type, const char* suffix);
// The summary's bars (SummaryBar / IvBar): value of max, 6 px tall.
void ui_bar(struct pd_canvas* c, int x, int y, int w, long value, long max, uint32_t color);
// The party menu's held-item marker, 8x8 at (x, y); nothing for item 0 or
// before the ROM has supplied the art.
void ui_held_marker(struct pd_canvas* c, const struct pd_game* g, int x, int y, int item);

void ui_party_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
void ui_battle_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
void ui_bag_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
void ui_settings_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                     const struct pd_host_info* host);
void ui_map_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
void ui_guide_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
void ui_dex_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
void ui_overlay(struct pd_ui* ui, struct pd_canvas* c);
// Overlays drawn by their tab's file (PLACES, the GUIDE notice).
void ui_places_overlay(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g);
void ui_guide_notice(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g);

// A tap each tab's file handles; true if it was theirs.
bool ui_map_act(struct pd_ui* ui, int id);
bool ui_dex_act(struct pd_ui* ui, int id);
bool ui_guide_act(struct pd_ui* ui, int id);
bool ui_battle_act(struct pd_ui* ui, int id);
bool ui_card_act(struct pd_ui* ui, int id);
bool ui_bag_act(struct pd_ui* ui, int id);
// The game actions' overlays and taps (pd_ui_act.c).
void ui_act_overlay(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
bool ui_act_act(struct pd_ui* ui, int id);
void ui_card_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
bool ui_card_tick(struct pd_ui* ui, unsigned ms);

// A pill of white text (VerdictPill), 14 px tall; returns its width.
int ui_pill(struct pd_canvas* c, int x, int y, const char* text, uint32_t fill);
// The pixel star (PixelIcons.star), 9x9; the favorites' gold.
#define UI_FAVORITE 0xE0A010
void ui_star(struct pd_canvas* c, int x, int y, uint32_t color);

// The lists' caught / seen mark: a Poké Ball, or a grey one.
void ui_ball(struct pd_canvas* c, int x, int y, bool caught);

// A scrolled list's offset clamped to its content: max(0, total - view).
int ui_clamp_scroll(int scroll, int total, int view);
// A thin scroll bar along a list's right edge.
void ui_scroll_bar(struct pd_canvas* c, int x, int y, int h, int scroll, int total, uint32_t color);

#endif
