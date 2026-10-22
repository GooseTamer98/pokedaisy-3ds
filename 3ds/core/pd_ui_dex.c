// POKéDEX: the list (national or the game's regional dex) with the save's
// seen / caught marks, and an entry page read from the ROM - the app's
// PokedexScreen, fitted to 320x212. The entry keeps its header (picture, No.,
// category, types, size) and shows one of the app's sections under it: INFO
// (text, base stats, ability, egg groups, gender, catch rate), EVOLVE (the
// whole family, a tap opens one), AREA (where it's met) or MOVES (level-up,
// then TM / HM).
#include <stdio.h>
#include <string.h>

#include "pd_dex.h"
#include "pd_evo.h"
#include "pd_guide.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

#define TITLE_H 24
#define LIST_Y (CONTENT_Y + TITLE_H + 4)
#define DEX_ROW 18
#define TOGGLE_W 78

// Whether the National Dex shows: the player's pick, else the save's own.
static bool national_view(const struct pd_ui* ui, const struct pd_snapshot* s) {
    return ui->dexNational >= 0 ? ui->dexNational : s->dexNational;
}

// The list: national numbers in the order shown; returns how many.
static int dex_list(const struct pd_game* g, bool national, int* out) {
    int n = 0;
    if (national) {
        for (int i = 1; i <= PD_NATIONAL_COUNT; i++) {
            if (pd_dex_species(g, i)) out[n++] = i;
        }
    } else {
        for (int i = 1; i <= g->cfg->regionalCount; i++) {
            int nat = pd_dex_regional(g, i);
            if (nat) out[n++] = nat;
        }
    }
    return n;
}

// The number shown for an entry: its regional position in a regional dex.
static int shown_number(const struct pd_game* g, bool national, int nat) {
    if (national) return nat;
    for (int i = 1; i <= g->cfg->regionalCount; i++) {
        if (pd_dex_regional(g, i) == nat) return i;
    }
    return nat;
}

static void title(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                  const int* list, int n, bool national) {
    int x = MARGIN, w = c->w - 2 * MARGIN - TOGGLE_W - 4;
    int f = pd_title_box(c, x, CONTENT_Y, w, TITLE_H, PD_TITLE_FILL);
    int ty = pd_text_y(CONTENT_Y, TITLE_H);
    pd_text(c, x + f + 6, ty, "POKéDEX", PD_TITLE_TEXT, PD_TITLE_SHADOW);
    int seen = 0, caught = 0;
    for (int i = 0; i < n; i++) {
        seen += pd_dex_seen(s, list[i]);
        caught += pd_dex_caught(s, list[i]);
    }
    char counts[40];
    snprintf(counts, sizeof(counts), "SEEN %d  OWN %d", seen, caught);
    pd_text_right(c, x + w - f - 6, ty, counts, PD_VALUE, PD_VALUE_SHADOW);
    // Which dex: tap for the other one.
    char other[24];
    snprintf(other, sizeof(other), "%s", national ? g->cfg->regionName : "NATIONAL");
    ui_button(ui, c, x + w + 4, CONTENT_Y, TOGGLE_W, TITLE_H, other, HIT_DEX_TOGGLE);
}

// The list shown (rebuilt each draw), kept for the tap handler, which has no
// game at hand: the toggle flips what shows, up / down step through it.
static int lastList[PD_NATIONAL_COUNT], lastCount;
static bool lastNational;

static void list_view(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    const int* list = lastList;
    int n = lastCount;
    bool national = lastNational;
    title(ui, c, g, s, list, n, national);

    int x = MARGIN, w = c->w - 2 * MARGIN;
    int f = pd_list_box(c, x, LIST_Y, w, c->h - LIST_Y - MARGIN);
    int ix = x + f, iy = LIST_Y + f + 1, iw = w - 2 * f, ih = c->h - LIST_Y - MARGIN - 2 * f - 2;
    ui_add_scroll_hit(ui, ix, iy, iw, ih, HIT_DEX_LIST);
    ui->dexScroll = ui_clamp_scroll(ui->dexScroll, n * DEX_ROW, ih);
    pd_clip(c, ix, iy, iw, ih);
    char buf[40], num[16];
    for (int i = ui->dexScroll / DEX_ROW; i < n; i++) {
        int ry = iy + i * DEX_ROW - ui->dexScroll;
        if (ry >= iy + ih) break;
        int nat = list[i];
        bool seen = pd_dex_seen(s, nat), caught = pd_dex_caught(s, nat);
        if (ui->pressedId == HIT_DEX_ROW + nat) pd_fill(c, ix, ry, iw, DEX_ROW, PD_ROW_SELECTED);
        if (seen) ui_ball(c, ix + 6, ry + 4, caught);
        snprintf(num, sizeof(num), "No.%03d", shown_number(g, national, nat));
        int ty = pd_text_y(ry, DEX_ROW);
        pd_text(c, ix + 22, ty, num, PD_MUTED, PD_MUTED_SHADOW);
        const char* name = pd_species_name(pd_dex_species(g, nat), buf, sizeof(buf));
        pd_text(c, ix + 74, ty, name, seen ? PD_LABEL : PD_MUTED, seen ? PD_LABEL_SHADOW : PD_MUTED_SHADOW);
        int top = ry < iy ? iy : ry;
        int bottom = ry + DEX_ROW > iy + ih ? iy + ih : ry + DEX_ROW;
        ui_add_scroll_hit(ui, ix, top, iw, bottom - top, HIT_DEX_ROW + nat);
    }
    pd_unclip(c);
    ui_scroll_bar(c, ix + iw - 4, iy, ih, ui->dexScroll, n * DEX_ROW, PD_MUTED);
}

static void draw_sprite(struct pd_canvas* c, int x, int y, const uint32_t* px) {
    for (int py = 0; py < PD_SPRITE; py++) {
        for (int pxx = 0; pxx < PD_SPRITE; pxx++) {
            uint32_t v = px[py * PD_SPRITE + pxx];
            if (v != 0xFF000000u) pd_fill(c, x + pxx, y + py, 1, 1, v);
        }
    }
}

// --- the entry's sections, as lines a drag scrolls ---

#define LINE_H 16
#define MAX_LINES 160

enum line_kind { LINE_HEAD, LINE_PLACE, LINE_ROW, LINE_NOTE };
struct line {
    uint8_t kind;
    uint8_t indent;
    bool current;  // EVOLVE: the species open
    int national;  // EVOLVE: tapping opens it (0 = no tap)
    int move;      // MOVES: the move (its type chip at the right)
    char left[48], right[40];
};
static struct line lines[MAX_LINES];
static int lineCount;

static struct line* add_line(enum line_kind kind, int indent, const char* left, const char* right) {
    if (lineCount >= MAX_LINES) return NULL;
    struct line* l = &lines[lineCount++];
    memset(l, 0, sizeof(*l));
    l->kind = (uint8_t) kind;
    l->indent = (uint8_t) indent;
    snprintf(l->left, sizeof(l->left), "%s", left ? left : "");
    snprintf(l->right, sizeof(l->right), "%s", right ? right : "");
    return l;
}

// EVOLVE: the whole family both ways, from its first stage down, each with
// how it evolves from the one above.
static void family_from(const struct pd_game* g, int species, int depth, int open, const struct pd_evo* how) {
    char name[32], text[80] = "";
    if (how) pd_evo_how(g, how, text, sizeof(text));
    struct line* l = add_line(LINE_ROW, depth, pd_species_name(species, name, sizeof(name)), text);
    if (!l) return;
    l->national = pd_dex_national(g, species);
    l->current = species == open;
    struct pd_evo evos[PD_EVOS_PER_MON];
    int n = pd_evolutions(g, species, evos);
    for (int i = 0; i < n && depth < 3; i++) family_from(g, evos[i].target, depth + 1, open, &evos[i]);
}

static void build_evolve(const struct pd_game* g, int species) {
    if (!pd_evo_available(g)) {
        add_line(LINE_NOTE, 0, "This game's evolutions aren't read yet.", NULL);
        return;
    }
    int base = species;
    for (int up = 0; up < 3; up++) {
        int from = pd_evolves_from(g, base, NULL);
        if (!from) break;
        base = from;
    }
    struct pd_evo evos[PD_EVOS_PER_MON];
    if (base == species && !pd_evolutions(g, species, evos)) {
        add_line(LINE_NOTE, 0, "Doesn't evolve.", NULL);
        return;
    }
    family_from(g, base, 0, species, NULL);
}

// AREA: one block per map section, a line per method, then gifts, eggs and
// in-game trades from the GUIDE's area data (areaPlaces).
static void build_area(const struct pd_game* g, int species) {
    static struct pd_catch_spot spots[96];
    int n = pd_catch_spots(g, species, spots, 96);
    int things;
    const struct pd_area_thing* area = pd_areas_for(g, &things);
    int secs[128], secCount = 0;
    for (int i = 0; i < n; i++) {
        bool dup = false;
        for (int k = 0; k < secCount && !dup; k++) dup = secs[k] == spots[i].mapsec;
        if (!dup && secCount < 128) secs[secCount++] = spots[i].mapsec;
    }
    for (int i = 0; i < things; i++) {
        const struct pd_area_thing* t = &area[i];
        if (t->id != species || (t->kind != PD_AREA_MON && t->kind != PD_AREA_EGG && t->kind != PD_AREA_TRADE)) continue;
        bool dup = false;
        for (int k = 0; k < secCount && !dup; k++) dup = secs[k] == t->mapsec;
        if (!dup && secCount < 128) secs[secCount++] = t->mapsec;
    }
    if (!secCount) {
        add_line(LINE_NOTE, 0, pd_guide_rom_ok(g) ? "Not found in the wild." : "This game's wild tables aren't read yet.",
                 NULL);
        return;
    }
    char buf[40], name[32];
    for (int k = 0; k < secCount; k++) {
        add_line(LINE_PLACE, 0, pd_mapsec_name(g, secs[k]), NULL);
        for (int m = 0; m < PD_WILD_METHODS; m++) {
            for (int i = 0; i < n; i++) {
                const struct pd_catch_spot* sp = &spots[i];
                if (sp->mapsec != secs[k] || sp->method != m) continue;
                if (sp->minLv == sp->maxLv) snprintf(buf, sizeof(buf), "LV %d  %d%%", sp->minLv, sp->pct);
                else snprintf(buf, sizeof(buf), "LV %d-%d  %d%%", sp->minLv, sp->maxLv, sp->pct);
                add_line(LINE_ROW, 1, pd_wild_method_name(m), buf);
            }
        }
        for (int i = 0; i < things; i++) {
            const struct pd_area_thing* t = &area[i];
            if (t->id != species || t->mapsec != secs[k]) continue;
            char what[48];
            if (t->kind == PD_AREA_EGG) snprintf(what, sizeof(what), "AN EGG");
            else if (t->kind == PD_AREA_TRADE) snprintf(what, sizeof(what), "TRADE FOR %s", pd_species_name(t->wants, name, sizeof(name)));
            else if (t->kind == PD_AREA_MON) snprintf(what, sizeof(what), "GIFT");
            else continue;
            add_line(LINE_ROW, 1, what, t->where);
        }
    }
}

// MOVES: LEVEL UP (level, move, type), then TM / HM.
static void build_moves(const struct pd_game* g, int species) {
    static struct pd_level_move lv[64];
    static struct pd_tm_move tm[64];
    char lab[16];
    add_line(LINE_HEAD, 0, "LEVEL UP", NULL);
    int n = pd_level_moves(g, species, lv, 64);
    if (!n) add_line(LINE_NOTE, 0, "NONE", NULL);
    for (int i = 0; i < n; i++) {
        if (lv[i].level == 0) snprintf(lab, sizeof(lab), "EVO.");
        else snprintf(lab, sizeof(lab), "LV %d", lv[i].level);
        struct line* l = add_line(LINE_ROW, 0, lab, NULL);
        if (l) l->move = lv[i].move;
    }
    n = pd_tm_moves(g, species, tm, 64);
    if (n < 0) {
        add_line(LINE_NOTE, 0, "This game's TMs aren't read yet.", NULL);
        return;
    }
    add_line(LINE_HEAD, 0, "TM / HM", NULL);
    if (!n) add_line(LINE_NOTE, 0, "NONE", NULL);
    for (int i = 0; i < n; i++) {
        struct line* l = add_line(LINE_ROW, 0, tm[i].label, NULL);
        if (l) l->move = tm[i].move;
    }
}

static void draw_lines(struct pd_ui* ui, struct pd_canvas* c, int x, int y, int w, int h) {
    int total = lineCount * LINE_H;
    ui->dexSectionScroll = ui_clamp_scroll(ui->dexSectionScroll, total, h);
    ui_add_scroll_hit(ui, x, y, w, h, HIT_DEX_SECTION_LIST);
    pd_clip(c, x, y, w, h);
    char buf[40];
    for (int i = ui->dexSectionScroll / LINE_H; i < lineCount; i++) {
        const struct line* l = &lines[i];
        int ly = y + i * LINE_H - ui->dexSectionScroll;
        if (ly >= y + h) break;
        int lx = x + l->indent * 10;
        int right = x + w - 8;
        switch (l->kind) {
        case LINE_HEAD: pd_text(c, lx, ly, l->left, PD_MUTED, PD_MUTED_SHADOW); break;
        case LINE_NOTE: pd_text_fit(c, lx, ly, right - lx, l->left, PD_MUTED, PD_MUTED_SHADOW); break;
        case LINE_PLACE: pd_text_fit(c, lx, ly, right - lx, l->left, PD_LABEL, PD_LABEL_SHADOW); break;
        default:
            if (l->move) {
                // A move line: its label (LV 7, TM06), the name, the type chip.
                pd_text(c, lx, ly, l->left, PD_VALUE, PD_VALUE_SHADOW);
                int cw = pd_text_width(pd_type_name(pd_move_type(l->move))) + 8;
                pd_text_fit(c, lx + 44, ly, right - cw - 6 - lx - 44, pd_move_name(l->move, buf, sizeof(buf)),
                            PD_LABEL, PD_LABEL_SHADOW);
                ui_type_chip(c, right - cw, ly + 2, pd_move_type(l->move), NULL);
            } else {
                int rw = *l->right ? pd_text_width(l->right) : 0;
                if (rw > (right - lx) / 2) rw = (right - lx) / 2;
                pd_text_fit(c, lx, ly, right - lx - rw - 6, l->left, l->current ? PD_VALUE : PD_LABEL,
                            l->current ? PD_VALUE_SHADOW : PD_LABEL_SHADOW);
                if (*l->right) pd_text_fit(c, right - rw, ly, rw, l->right, l->national ? PD_MUTED : PD_VALUE,
                                           l->national ? PD_MUTED_SHADOW : PD_VALUE_SHADOW);
            }
            break;
        }
        if (l->national && !l->current) ui_add_scroll_hit(ui, x, ly, w, LINE_H, HIT_DEX_ROW + l->national);
    }
    pd_unclip(c);
    ui_scroll_bar(c, x + w - 4, y, h, ui->dexSectionScroll, total, PD_MUTED);
}

// INFO: the dex text, the base stats and their total, then ABILITY, EGG
// GROUP, GENDER and CATCH RATE (the app's InfoLines), scrolled as one.
static void draw_info(struct pd_ui* ui, struct pd_canvas* c, const struct pd_dex_entry* e, int x, int y, int w, int h) {
    char buf[64];
    int textLines = pd_text_wrap(NULL, 0, 0, w - 8, LINE_H, 8, e->description, 0, 0);
    int total = textLines * LINE_H + 2 + 7 * LINE_H;
    ui->dexSectionScroll = ui_clamp_scroll(ui->dexSectionScroll, total, h);
    ui_add_scroll_hit(ui, x, y, w, h, HIT_DEX_SECTION_LIST);
    pd_clip(c, x, y, w, h);
    int dy = y - ui->dexSectionScroll;
    pd_text_wrap(c, x, dy, w - 8, LINE_H, 8, e->description, PD_LABEL, PD_LABEL_SHADOW);
    dy += textLines * LINE_H + 2;
    static const char* const STAT[6] = { "HP", "ATK", "DEF", "SPA", "SPD", "SPE" };
    int sx = x, sum = 0;
    for (int i = 0; i < 6; i++) {
        sx += pd_text(c, sx, dy, STAT[i], PD_MUTED, PD_MUTED_SHADOW) + 3;
        snprintf(buf, sizeof(buf), "%d", e->stats[i]);
        sx += pd_text(c, sx, dy, buf, PD_LABEL, PD_LABEL_SHADOW) + 7;
        sum += e->stats[i];
    }
    dy += LINE_H + 2;
    snprintf(buf, sizeof(buf), "%d", sum);
    ui_row(c, x, dy, w - 8, "TOTAL", buf, false);
    dy += LINE_H;
    if (e->ability2[0]) snprintf(buf, sizeof(buf), "%s / %s", e->ability1, e->ability2);
    else snprintf(buf, sizeof(buf), "%s", e->ability1);
    ui_row(c, x, dy, w - 8, "ABILITY", buf, false);
    dy += LINE_H;
    const char *e1 = pd_egg_group_name(e->eggGroup1), *e2 = pd_egg_group_name(e->eggGroup2);
    if (*e1 && *e2 && e->eggGroup2 != e->eggGroup1) snprintf(buf, sizeof(buf), "%s / %s", e1, e2);
    else snprintf(buf, sizeof(buf), "%s", e1);
    if (*buf) {
        ui_row(c, x, dy, w - 8, "EGG GROUP", buf, false);
        dy += LINE_H;
    }
    // genderLabel: every Gen 3 ratio is a multiple of 12.5% (PERCENT_FEMALE).
    if (e->genderRatio == 255) snprintf(buf, sizeof(buf), "UNKNOWN");
    else if (e->genderRatio == 254) snprintf(buf, sizeof(buf), "♀ ONLY");
    else if (e->genderRatio == 0) snprintf(buf, sizeof(buf), "♂ ONLY");
    else {
        int female = (int) ((e->genderRatio * 100 / 255.0) / 12.5 + 0.5) * 125; // tenths of a percent
        int male = 1000 - female;
        snprintf(buf, sizeof(buf), "♂ %d%s%%  ♀ %d%s%%", male / 10, male % 10 ? ".5" : "", female / 10,
                 female % 10 ? ".5" : "");
    }
    ui_row(c, x, dy, w - 8, "GENDER", buf, false);
    dy += LINE_H;
    snprintf(buf, sizeof(buf), "%d", e->catchRate);
    ui_row(c, x, dy, w - 8, "CATCH RATE", buf, false);
    pd_unclip(c);
    ui_scroll_bar(c, x + w - 4, y, h, ui->dexSectionScroll, total, PD_MUTED);
}

static void entry_view(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    static struct pd_dex_entry e;
    static uint32_t sprite[PD_SPRITE * PD_SPRITE];
    static bool spriteOk;
    static int loaded, loadedSection = -1;
    static unsigned loadedRom;
    bool changed = loaded != ui->dexOpen || loadedRom != pd_rom_id(g);
    if (changed) {
        pd_dex_entry(g, ui->dexOpen, &e);
        spriteOk = pd_dex_sprite(g, e.species, sprite);
        loaded = ui->dexOpen;
        loadedRom = pd_rom_id(g);
        ui->dexSectionScroll = 0;
    }
    if (changed || loadedSection != ui->dexSection) {
        // The section's lines, built once per entry and section.
        lineCount = 0;
        if (ui->dexSection == PD_DEX_EVOLVE) build_evolve(g, e.species);
        else if (ui->dexSection == PD_DEX_AREA) build_area(g, e.species);
        else if (ui->dexSection == PD_DEX_MOVES) build_moves(g, e.species);
        if (loadedSection != ui->dexSection) ui->dexSectionScroll = 0;
        loadedSection = ui->dexSection;
    }
    bool national = lastNational;
    char buf[64], buf2[40];
    int x = MARGIN, y = CONTENT_Y, w = c->w - 2 * MARGIN, h = c->h - CONTENT_Y - MARGIN;
    int f = pd_title_box(c, x, y, w, h, PD_TITLE_FILL);
    int ix = x + f + 4, iw = w - 2 * (f + 4);
    int top = y + f + 2;

    // The sprite on the list window's grey, framed.
    int bx = ix, by = top;
    pd_list_box(c, bx, by, PD_SPRITE + 16, PD_SPRITE + 16);
    if (spriteOk) draw_sprite(c, bx + 8, by + 8, sprite);

    int tx = bx + PD_SPRITE + 24, tw = ix + iw - tx;
    snprintf(buf, sizeof(buf), "No.%03d", shown_number(g, national, e.national));
    pd_text(c, tx, top, buf, PD_MUTED, PD_MUTED_SHADOW);
    int nw = pd_text(c, tx + 50, top, pd_species_name(e.species, buf2, sizeof(buf2)), PD_LABEL, PD_LABEL_SHADOW);
    if (pd_dex_seen(s, e.national)) ui_ball(c, tx + 50 + nw + 6, top + 4, pd_dex_caught(s, e.national));
    // Step through the list (wrapping): the arrows at the header's right.
    int ax = ix + iw - 2 * 30 + 4;
    ui_button(ui, c, ax, top - 2, 26, 18, "", HIT_DEX_PREV);
    pd_triangle(c, ax + 9, top + 3, 5, PD_UP, PD_TITLE_TEXT);
    ui_button(ui, c, ax + 30, top - 2, 26, 18, "", HIT_DEX_NEXT);
    pd_triangle(c, ax + 39, top + 4, 5, PD_DOWN, PD_TITLE_TEXT);
    snprintf(buf, sizeof(buf), "%s POKéMON", e.category);
    pd_text_fit(c, tx, top + 18, tw, buf, PD_MUTED, PD_MUTED_SHADOW);
    int cx = tx;
    cx += ui_type_chip(c, cx, top + 38, e.type1, NULL) + 4;
    if (e.type2 != e.type1 && e.type2 != PD_TYPE_NONE) ui_type_chip(c, cx, top + 38, e.type2, NULL);
    char ht[16], wt[20];
    pd_dex_height(e.heightDm, ht, sizeof(ht));
    pd_dex_weight(e.weightHg, wt, sizeof(wt));
    snprintf(buf, sizeof(buf), "HT %s   WT %s", ht, wt);
    pd_text_fit(c, tx, top + 56, tw, buf, PD_LABEL, PD_LABEL_SHADOW);

    // The section, between the header and the bar.
    int byBtn = y + h - f - 26;
    int sy = by + PD_SPRITE + 20;
    pd_fill(c, ix, sy - 3, iw, 1, PD_DIVIDER);
    if (ui->dexSection == PD_DEX_INFO) draw_info(ui, c, &e, ix, sy, iw, byBtn - 4 - sy);
    else draw_lines(ui, c, ix, sy, iw, byBtn - 4 - sy);

    // The bar: the sections (the open one lit, like the app's chips), BACK.
    static const char* const SECTIONS[PD_DEX_SECTIONS] = { "INFO", "EVOLVE", "AREA", "MOVES" };
    int chipW = (iw - 66 - 4 * 4) / PD_DEX_SECTIONS;
    for (int i = 0; i < PD_DEX_SECTIONS; i++) {
        int sx = ix - 2 + i * (chipW + 4);
        if (i == ui->dexSection) {
            pd_title_box(c, sx, byBtn, chipW, 22, PD_TITLE_FILL);
            pd_text(c, sx + (chipW - pd_text_width(SECTIONS[i])) / 2, pd_text_y(byBtn, 22), SECTIONS[i], PD_VALUE,
                    PD_VALUE_SHADOW);
        } else {
            ui_button(ui, c, sx, byBtn, chipW, 22, SECTIONS[i], HIT_DEX_SECTION + i);
        }
    }
    ui_button(ui, c, x + w - f - 66, byBtn, 62, 22, "BACK", HIT_BACK);
}

void ui_dex_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    if (!pd_dex_available(g)) {
        ui_notice(c, "NO POKéDEX YET", "This version's POKéDEX tables", "aren't in the 3DS companion yet.");
        return;
    }
    if (!s->dexOk) {
        ui_notice(c, "NO POKéDEX YET", "The POKéDEX shows once the game", "has loaded a save.");
        return;
    }
    lastNational = national_view(ui, s);
    lastCount = dex_list(g, lastNational, lastList);
    if (ui->dexOpen) {
        entry_view(ui, c, g, s);
    } else {
        list_view(ui, c, g, s);
    }
}

bool ui_dex_act(struct pd_ui* ui, int id) {
    if (ui->tab != PD_TAB_DEX || ui->overlay != PD_OVERLAY_NONE) return false;
    if (id == HIT_DEX_TOGGLE) {
        // Flip from what shows now (the save's default until something was picked).
        ui->dexNational = !lastNational;
        ui->dexScroll = 0;
        return true;
    }
    if (id >= HIT_DEX_SECTION && id < HIT_DEX_SECTION + PD_DEX_SECTIONS) {
        ui->dexSection = id - HIT_DEX_SECTION;
        return true;
    }
    if (id >= HIT_DEX_ROW && id <= HIT_DEX_ROW + PD_NATIONAL_COUNT) {
        ui->dexOpen = id - HIT_DEX_ROW;
        return true;
    }
    if ((id == HIT_DEX_PREV || id == HIT_DEX_NEXT) && lastCount > 0) {
        int at = 0;
        for (int i = 0; i < lastCount; i++) {
            if (lastList[i] == ui->dexOpen) at = i;
        }
        at = (at + lastCount + (id == HIT_DEX_NEXT ? 1 : -1)) % lastCount;
        ui->dexOpen = lastList[at];
        return true;
    }
    return false;
}
