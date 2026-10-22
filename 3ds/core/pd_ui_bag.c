// BAG: ALL, FAVORITES (once something is starred) and the game's pockets in
// its own order, SORT, the items in a list a drag scrolls, the tapped item's
// icon and description with its star, and for a TM who in the party can learn
// it - in each game's bag colours (FireRedBag / EmeraldBag, ItemsScreen.kt).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pd_action.h"
#include "pd_advice.h"
#include "pd_icon.h"
#include "pd_settings.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

struct bag_palette {
    struct pd_layer panel[4];
    int panelLayers;
    int tabLayers; // the header reuses this many panel layers, filled with the next one's colour
    uint32_t fill, divider; // divider 0 = none
    uint32_t text, textShadow;
    uint32_t tabText, tabTextShadow;
    uint32_t cursor;
    struct pd_layer desc[2];
    int descLayers;
    uint32_t descFill, descText, descTextShadow;
    uint32_t backdropA, backdropB; // 0 = the companion's own backdrop
};

static const struct bag_palette FIRERED_BAG = {
    .panel = { { 0x686868, 2 }, { 0xE8E0A8, 1 }, { 0xF0C870, 2 }, { 0xD0B050, 2 } },
    .panelLayers = 4,
    .tabLayers = 2,
    .fill = 0xF8F8C8, .divider = 0xE8E0A8,
    .text = 0x606060, .textShadow = 0xD0D0C8,
    .tabText = 0xF8F8F8, .tabTextShadow = 0x606060,
    .cursor = 0xD88848,
    .desc = { { 0x005070, 2 }, { 0x10A8D8, 1 } },
    .descLayers = 2,
    .descFill = 0x0078C0, .descText = 0xF8F8F8, .descTextShadow = 0x606060,
};

static const struct bag_palette EMERALD_BAG = {
    .panel = { { 0x335170, 2 }, { 0x2C3941, 1 }, { 0x636372, 1 }, { 0xFBE898, 2 } },
    .panelLayers = 4,
    .tabLayers = 3,
    .fill = 0xFFFFD3, .divider = 0,
    .text = 0x000000, .textShadow = 0xD6D6CF,
    .tabText = 0x000000, .tabTextShadow = 0xB89D63,
    .cursor = 0xEC5F2A,
    .desc = { { 0x7B7B7B, 1 } },
    .descLayers = 1,
    .descFill = 0xFFFFFF, .descText = 0x000000, .descTextShadow = 0xD6D6CF,
    .backdropA = 0xC871F7, .backdropB = 0x5882F7,
};

#define HEADER_H 24
#define SORT_W 78
#define PC_W 30
#define ACT_W 44
#define LIST_Y (CONTENT_Y + HEADER_H + 4)
#define LIST_H 118
#define LEARNERS_H 48
#define BAG_ROW 17
#define FIRST_TM 289 // ITEM_TM01..ITEM_HM08
#define FIRST_HM 339 // ITEM_HM01..ITEM_HM08: no quantity, like the game
#define LAST_HM 346
#define LEARN_GREEN 0x389830
#define LEARNED_BLUE 0x4878C8
#define VERDICT_GREY 0x8C8C94

static const char* const SORT_NAMES[PD_SORTS] = { "DEFAULT", "NAME", "TYPE", "COUNT" };

struct row {
    uint16_t id, quantity;
    uint8_t pocket, order; // its pocket, and that pocket's place in the game's order
    int index;             // its place in the bag (ALL: pocket by pocket)
    char name[28];
    bool favorite;
};

static const char* tab_name(int tab) {
    if (tab == PD_BAG_ALL) return "ALL";
    if (tab == PD_BAG_FAVORITES) return "FAVORITES";
    return pd_pocket_name(tab - PD_BAG_POCKET);
}

// The bag's name for a row: a TM / HM with its move ("TM06 TOXIC", the app's
// tmMoveNames), else the game's, else "ITEM n".
static void row_name(const struct pd_game* g, int id, char* buf, size_t len) {
    const char* name = pd_item_name(g, id);
    int move = pd_tm_item_move(g, id);
    char mv[24];
    if (move && *name) snprintf(buf, len, "%s %s", name, pd_move_name(move, mv, sizeof(mv)));
    else if (*name) snprintf(buf, len, "%s", name);
    else snprintf(buf, len, "ITEM %d", id);
}

static int sortMode;
static int by_sort(const void* x, const void* y) {
    const struct row *a = x, *b = y;
    int d = 0;
    switch (sortMode) {
    case PD_SORT_DEFAULT: d = b->favorite - a->favorite; break; // starred first, the bag's order kept
    case PD_SORT_NAME: d = strcmp(a->name, b->name); break;
    case PD_SORT_TYPE: d = a->order - b->order; if (!d) d = strcmp(a->name, b->name); break;
    case PD_SORT_COUNT: d = b->quantity - a->quantity; if (!d) d = strcmp(a->name, b->name); break;
    }
    return d ? d : a->index - b->index;
}

// The rows the open tab lists, in the chosen order.
static int gather(struct pd_ui* ui, const struct pd_game* g, const struct pd_snapshot* s,
                  const struct pd_favorites* fav, struct row* rows) {
    int n = 0;
    for (int o = 0; o < PD_POCKET_COUNT; o++) {
        int pocket = g->cfg->bagOrder[o];
        if (ui->bagTab >= PD_BAG_POCKET && pocket != ui->bagTab - PD_BAG_POCKET) continue;
        const struct pd_pocket_items* items = &s->bag[pocket];
        for (int i = 0; i < items->count; i++) {
            const struct pd_item* it = &items->items[i];
            bool f = pd_favorite(fav, it->id);
            if (ui->bagTab == PD_BAG_FAVORITES && !f) continue;
            struct row* r = &rows[n];
            r->id = it->id;
            r->quantity = it->quantity;
            r->pocket = (uint8_t) pocket;
            r->order = (uint8_t) o;
            r->index = n;
            r->favorite = f;
            row_name(g, it->id, r->name, sizeof(r->name));
            n++;
        }
    }
    sortMode = ui->bagSort;
    qsort(rows, (size_t) n, sizeof(rows[0]), by_sort);
    return n;
}

static void header(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct bag_palette* p) {
    // PC anywhere (the app's BAG bar PC): left of SORT where the ROM has it.
    bool pc = pd_act_can_pc(g);
    int x = MARGIN, w = c->w - 2 * MARGIN - SORT_W - 4 - (pc ? PC_W + 4 : 0);
    pd_layered_box(c, x, CONTENT_Y, w, HEADER_H, 3, p->panel, p->tabLayers, p->panel[p->tabLayers].color);
    const char* name = tab_name(ui->bagTab);
    int ty = pd_text_y(CONTENT_Y, HEADER_H);
    pd_text(c, x + (w - pd_text_width(name)) / 2, ty, name, p->tabText, p->tabTextShadow);
    // The game's pocket arrows, drawn (the font has no ◀ / ▶).
    int ay = CONTENT_Y + HEADER_H / 2 - 5;
    uint32_t prev = ui->pressedId == HIT_POCKET_PREV ? p->cursor : p->tabText;
    uint32_t next = ui->pressedId == HIT_POCKET_NEXT ? p->cursor : p->tabText;
    pd_triangle(c, x + 10, ay, 6, PD_LEFT, prev);
    pd_triangle(c, x + w - 16, ay, 6, PD_RIGHT, next);
    ui_add_hit(ui, x, CONTENT_Y, 48, HEADER_H, HIT_POCKET_PREV);
    ui_add_hit(ui, x + w - 48, CONTENT_Y, 48, HEADER_H, HIT_POCKET_NEXT);
    int sx = x + w + 4;
    if (pc) {
        bool down = ui->pressedId == HIT_ACT_PC;
        pd_layered_box(c, sx, CONTENT_Y, PC_W, HEADER_H, 3, p->panel, p->tabLayers,
                       down ? p->cursor : p->panel[p->tabLayers].color);
        pd_text(c, sx + (PC_W - pd_text_width("PC")) / 2, ty, "PC", p->tabText, p->tabTextShadow);
        ui_add_hit(ui, sx, CONTENT_Y, PC_W, HEADER_H, HIT_ACT_PC);
        sx += PC_W + 4;
    }
    // SORT: a tap steps DEFAULT / NAME / TYPE / COUNT, like the app's button.
    bool pressed = ui->pressedId == HIT_BAG_SORT;
    pd_layered_box(c, sx, CONTENT_Y, SORT_W, HEADER_H, 3, p->panel, p->tabLayers,
                   pressed ? p->cursor : p->panel[p->tabLayers].color);
    // Its icon: three bars, longest first.
    for (int i = 0; i < 3; i++) pd_fill(c, sx + 7, CONTENT_Y + 7 + i * 4, 9 - i * 3, 2, p->tabText);
    pd_text(c, sx + 19, ty, SORT_NAMES[ui->bagSort], p->tabText, p->tabTextShadow);
    ui_add_hit(ui, sx, CONTENT_Y, SORT_W, HEADER_H, HIT_BAG_SORT);
}

static void list(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct bag_palette* p,
                 const struct row* rows, int n, int listH) {
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int f = pd_layered_box(c, x, LIST_Y, w, listH, 3, p->panel, p->panelLayers, p->fill);
    int ix = x + f, iy = LIST_Y + f + 1, iw = w - 2 * f, ih = listH - 2 * f - 2;
    ui_add_scroll_hit(ui, ix, iy, iw, ih, HIT_BAG_LIST);
    if (n == 0) {
        pd_text(c, ix + 14, pd_text_y(iy, BAG_ROW), "The pocket is empty.", p->text, p->textShadow);
        return;
    }
    if (ui->bagReveal) {
        // A selection the list moved (a new order, a shorter list) comes into view.
        for (int i = 0; i < n; i++) {
            if (rows[i].id != ui->bagSelected) continue;
            if (i * BAG_ROW < ui->bagScroll) ui->bagScroll = i * BAG_ROW;
            if ((i + 1) * BAG_ROW > ui->bagScroll + ih) ui->bagScroll = (i + 1) * BAG_ROW - ih;
        }
        ui->bagReveal = false;
    }
    ui->bagScroll = ui_clamp_scroll(ui->bagScroll, n * BAG_ROW, ih);

    pd_clip(c, ix, iy, iw, ih);
    for (int i = ui->bagScroll / BAG_ROW; i < n; i++) {
        int ry = iy + i * BAG_ROW - ui->bagScroll;
        if (ry >= iy + ih) break;
        const struct row* r = &rows[i];
        bool cursor = r->id == ui->bagSelected || ui->pressedId == HIT_BAG_ROW + i;
        if (cursor) pd_triangle(c, ix + 4, ry + 3, 6, PD_RIGHT, p->cursor);
        int ty = pd_text_y(ry, BAG_ROW);
        int nw = pd_text_fit(c, ix + 14, ty, iw - 78, r->name, p->text, p->textShadow);
        if (r->favorite) ui_star(c, ix + 14 + nw + 4, ry + 4, UI_FAVORITE);
        int qx = ix + iw - 10;
        if (r->pocket != PD_POCKET_KEY && !(r->id >= FIRST_HM && r->id <= LAST_HM)) {
            char qty[12];
            snprintf(qty, sizeof(qty), "×%d", r->quantity);
            pd_text_right(c, qx, ty, qty, p->text, p->textShadow);
            qx -= pd_text_width(qty);
        }
        // The app's mark: something to do with it here (USE or GIVE).
        if (pd_act_can_use(g, r->id) || pd_act_can_give(g, r->id)) ui_ball(c, qx - 14, ry + 4, true);
        if (p->divider) pd_fill(c, ix + 4, ry + BAG_ROW - 1, iw - 8, 1, p->divider);
        // Rows cut off at the window's edge still take a tap on what shows.
        int top = ry < iy ? iy : ry;
        int bottom = ry + BAG_ROW > iy + ih ? iy + ih : ry + BAG_ROW;
        ui_add_scroll_hit(ui, ix, top, iw, bottom - top, HIT_BAG_ROW + i);
    }
    pd_unclip(c);
    ui_scroll_bar(c, ix + iw - 4, iy, ih, ui->bagScroll, n * BAG_ROW, p->cursor);
}

// TmLearnersRow: each party Pokémon's icon over ABLE / NOT ABLE / LEARNED, the
// best one BEST.
static void learners(struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                     const struct bag_palette* p, const struct pd_tm_learner* l, int n, int best, int y) {
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int f = pd_layered_box(c, x, y, w, LEARNERS_H, 3, p->panel, p->panelLayers, p->fill);
    int ix = x + f, iw = w - 2 * f;
    int colW = iw / n;
    if (colW > 60) colW = 60;
    int left = ix + (iw - colW * n) / 2;
    for (int i = 0; i < n; i++) {
        int cx = left + i * colW + colW / 2;
        pd_draw_mon_icon(c, g, s->party[i].species, cx - PD_ICON / 2, y + f - 2, l[i].fit == PD_TM_NOT_ABLE);
        const char* label = i == best ? "BEST" : l[i].fit == PD_TM_ABLE ? "ABLE"
            : l[i].fit == PD_TM_LEARNED ? "LEARNED" : "NO";
        uint32_t fill = i == best ? LEARN_GREEN : l[i].fit == PD_TM_NOT_ABLE ? VERDICT_GREY : LEARNED_BLUE;
        ui_pill(c, cx - (pd_text_width(label) + 6) / 2, y + LEARNERS_H - f - 16, label, fill);
    }
}

static void description(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                        const struct bag_palette* p, const struct row* sel, const struct pd_tm_learner* l, int best,
                        int y) {
    int x = MARGIN, w = c->w - 2 * MARGIN, h = c->h - y - MARGIN;
    int f = pd_layered_box(c, x, y, w, h, 3, p->desc, p->descLayers, p->descFill);
    int lines = (h - 2 * f) / 16;
    if (!sel) {
        pd_text_wrap(c, x + f + 6, y + f + 1, w - 2 * (f + 6), 16, lines, "Tap an item to read about it.", p->descText,
                     p->descTextShadow);
        return;
    }
    int tx = x + f + 6;
    if (pd_draw_item_icon(c, g, sel->id, x + f + 4, y + f + 4)) tx = x + f + 4 + PD_ITEM_ICON + 6;
    // USE / GIVE under the star, where the ROM can do them (FieldItems.kt).
    bool use = pd_act_can_use(g, sel->id), give = pd_act_can_give(g, sel->id);
    int bx = x + w - f - ACT_W - 2, by = y + f + 17;
    if (use) {
        ui_button(ui, c, bx, by, ACT_W, 16, "USE", HIT_ACT_USE);
        by += 17;
    }
    if (give) ui_button(ui, c, bx, by, ACT_W, 16, "GIVE", HIT_ACT_GIVE);
    // The star: a tap stars it for this game.
    int sx = x + w - f - 22;
    bool pressed = ui->pressedId == HIT_BAG_STAR;
    ui_star(c, sx + 6, y + f + 5, sel->favorite || pressed ? UI_FAVORITE : p->descTextShadow);
    ui_add_hit(ui, sx - 4, y, 26 + f, 24, HIT_BAG_STAR);
    char bestLine[64] = "";
    if (best >= 0) {
        char name[16], mv[24];
        snprintf(name, sizeof(name), "%s", s->party[best].nickname);
        if (l[best].advice.verdict == PD_REPLACE) {
            snprintf(bestLine, sizeof(bestLine), "BEST FOR %s: FORGET %s", name,
                     pd_move_name(l[best].advice.forget, mv, sizeof(mv)));
        } else {
            snprintf(bestLine, sizeof(bestLine), "BEST FOR %s", name);
        }
        lines--;
    }
    const char* text = pd_item_description(g, sel->id);
    if (!*text) text = sel->name;
    int right = use || give ? bx - 4 : sx - 2;
    pd_text_wrap(c, tx, y + f + 1, right - tx, 16, lines, text, p->descText, p->descTextShadow);
    if (*bestLine) {
        pd_text_fit(c, tx, y + f + 1 + lines * 16, x + w - f - 4 - tx, bestLine, p->descText, p->descTextShadow);
    }
}

void ui_bag_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    if (!s->bagOk) {
        ui_notice(c, "NO BAG YET", "The bag shows once the game", "has loaded a save.");
        return;
    }
    const struct bag_palette* p = g->kind == PD_GAME_EMERALD ? &EMERALD_BAG : &FIRERED_BAG;
    if (p->backdropA) {
        for (int y = CONTENT_Y - 2; y < c->h; y += 8) {
            pd_fill(c, 0, y, c->w, 8, ((y - CONTENT_Y + 2) / 8) % 2 ? p->backdropB : p->backdropA);
        }
    }
    const struct pd_favorites* fav = ui->settings ? pd_favorites_for(ui->settings, g, false) : NULL;
    // ALL, FAVORITES while the bag holds a starred item, then the pockets.
    bool anyFav = false;
    for (int pk = 0; pk < PD_POCKET_COUNT && !anyFav; pk++) {
        for (int i = 0; i < s->bag[pk].count && !anyFav; i++) anyFav = pd_favorite(fav, s->bag[pk].items[i].id);
    }
    ui->bagTabCount = 0;
    ui->bagTabs[ui->bagTabCount++] = PD_BAG_ALL;
    if (anyFav) ui->bagTabs[ui->bagTabCount++] = PD_BAG_FAVORITES;
    for (int o = 0; o < PD_POCKET_COUNT; o++) ui->bagTabs[ui->bagTabCount++] = PD_BAG_POCKET + g->cfg->bagOrder[o];
    if (ui->bagTab == PD_BAG_FAVORITES && !anyFav) ui->bagTab = PD_BAG_ALL;
    if (ui->bagTab < 0 || ui->bagTab >= PD_BAG_POCKET + PD_POCKET_COUNT) ui->bagTab = PD_BAG_ALL;
    if (ui->bagSort < 0 || ui->bagSort >= PD_SORTS) ui->bagSort = PD_SORT_DEFAULT;

    static struct row rows[PD_POCKET_COUNT * PD_POCKET_SLOTS];
    int n = gather(ui, g, s, fav, rows);
    const struct row* sel = NULL;
    ui->bagRows = n;
    for (int i = 0; i < n; i++) {
        ui->bagRowItem[i] = rows[i].id;
        if (rows[i].id == ui->bagSelected) sel = &rows[i];
    }
    if (!sel) ui->bagSelected = 0;

    // A TM: who can learn it, under a shorter list.
    static struct pd_tm_learner l[PD_PARTY_SIZE];
    int ln = sel ? pd_tm_learners(g, s, sel->id, l) : 0;
    int best = ln ? pd_tm_best(l, ln) : -1;
    int listH = ln ? LIST_H - LEARNERS_H - 4 : LIST_H;
    header(ui, c, g, p);
    list(ui, c, g, p, rows, n, listH);
    if (ln) learners(c, g, s, p, l, ln, best, LIST_Y + listH + 4);
    description(ui, c, g, s, p, sel, l, best, LIST_Y + LIST_H + 4);
}

bool ui_bag_act(struct pd_ui* ui, int id) {
    if (ui->tab != PD_TAB_BAG) return false;
    if (id == HIT_POCKET_PREV || id == HIT_POCKET_NEXT) {
        int at = 0;
        for (int i = 0; i < ui->bagTabCount; i++) {
            if (ui->bagTabs[i] == ui->bagTab) at = i;
        }
        int n = ui->bagTabCount ? ui->bagTabCount : 1;
        ui->bagTab = ui->bagTabs[(at + n + (id == HIT_POCKET_NEXT ? 1 : -1)) % n];
        ui->bagSelected = 0;
        ui->bagScroll = 0;
    } else if (id == HIT_BAG_SORT) {
        ui->bagSort = (ui->bagSort + 1) % PD_SORTS;
        ui->bagReveal = true;
    } else if (id == HIT_BAG_STAR) {
        if (ui->bagSelected > 0 && ui->game && ui->settings) {
            pd_favorite_toggle(ui->settings, ui->game, ui->bagSelected);
            ui->bagReveal = true;
            ui->action = PD_ACTION_SETTINGS_CHANGED;
        }
    } else if (id >= HIT_BAG_ROW && id < HIT_BAG_ROW + ui->bagRows) {
        ui->bagSelected = ui->bagRowItem[id - HIT_BAG_ROW];
        ui->bagReveal = true;
    } else {
        return false;
    }
    return true;
}
