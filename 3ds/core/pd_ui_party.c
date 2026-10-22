// PARTY: the six slots in FireRed's party colours, each with the game's own
// icon (from the ROM, pd_icon), and a summary on tap.
#include <stdio.h>
#include <string.h>

#include "pd_advice.h"
#include "pd_icon.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

// FireRedPartyPalette (PartyScreen.kt).
struct slot_colors {
    uint32_t outline, light, fill, shade;
};
static const struct slot_colors SLOT_NORMAL = { 0x4A4A63, 0x84C6DE, 0x3994DE, 0x297BB5 };
static const struct slot_colors SLOT_SELECTED = { 0xFF7331, 0xADEFFF, 0x7BD6EF, 0x4AADCE };
static const struct slot_colors SLOT_FAINTED = { 0x4A4A63, 0xD6A521, 0xC66B10, 0xA54A00 };
#define SLOT_TEXT 0xFFFFFF
#define SLOT_TEXT_SHADOW 0x737373
#define MALE 0x42CEFF
#define MALE_SHADOW 0x006394
#define FEMALE 0xFF9C94
#define FEMALE_SHADOW 0x9C4239

static const char* mon_display_name(const struct pd_mon* m, char* buf, size_t len) {
    if (m->species == PD_SPECIES_EGG) return "EGG";
    if (m->nickname[0]) return m->nickname;
    return pd_species_name(m->species, buf, len);
}

// The party menu's ♂ / ♀ after the name, in their own colours.
static void gender_mark(struct pd_canvas* c, int x, int y, enum pd_gender g) {
    if (g == PD_GENDER_MALE) pd_text(c, x, y, "♂", MALE, MALE_SHADOW);
    if (g == PD_GENDER_FEMALE) pd_text(c, x, y, "♀", FEMALE, FEMALE_SHADOW);
}

static void party_slot(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, int x, int y, int w,
                       int h, const struct pd_mon* m, int slot) {
    char buf[40];
    if (!m) {
        // An empty slot: just its outline over the backdrop.
        pd_round_rect(c, x, y, w, h, 5, SLOT_NORMAL.outline);
        pd_round_rect(c, x + 1, y + 1, w - 2, h - 2, 4, PD_BACKDROP_B);
        return;
    }
    bool egg = m->species == PD_SPECIES_EGG;
    bool fainted = !egg && m->hp == 0;
    const struct slot_colors* col = ui->pressedId == HIT_SLOT + slot ? &SLOT_SELECTED
        : fainted ? &SLOT_FAINTED : &SLOT_NORMAL;
    pd_round_rect(c, x, y, w, h, 5, col->outline);
    pd_round_rect(c, x + 1, y + 1, w - 2, h - 2, 4, col->light);
    pd_round_rect(c, x + 2, y + 2, w - 4, h - 4, 3, col->fill);
    pd_fill(c, x + 3, y + h - 6, w - 6, 3, col->shade);

    // The icon at the top left, like the game's party menu; the text beside it.
    int tx = x + 8;
    if (pd_draw_mon_icon(c, g, m->species, x + 2, y + 1, false)) {
        tx = x + 2 + PD_ICON + 3;
        // The party menu's sprite coords: the marker at the icon's (16, 22).
        ui_held_marker(c, g, x + 2 + 16, y + 1 + 22, m->heldItem);
    }
    const char* name = mon_display_name(m, buf, sizeof(buf));
    if (!egg) {
        char lv[12];
        snprintf(lv, sizeof(lv), "Lv%d", m->level);
        int lvW = pd_text_width(lv);
        pd_text_right(c, x + w - 8, y + 3, lv, SLOT_TEXT, SLOT_TEXT_SHADOW);
        int nameW = pd_text_fit(c, tx, y + 3, x + w - 8 - lvW - 6 - 10 - tx, name, SLOT_TEXT, SLOT_TEXT_SHADOW);
        gender_mark(c, tx + nameW + 3, y + 3, m->gender);
        ui_status_badge(c, tx, y + 24, m->status);
        char hp[16];
        snprintf(hp, sizeof(hp), "%d/%d", m->hp, m->maxHp);
        pd_text_right(c, x + w - 8, y + 21, hp, SLOT_TEXT, SLOT_TEXT_SHADOW);
        ui_hp_bar(c, x + 8, y + h - 18, w - 34, m->hp, m->maxHp);
    } else {
        pd_text_fit(c, tx, y + 3, x + w - 8 - tx, name, SLOT_TEXT, SLOT_TEXT_SHADOW);
    }
    ui_add_hit(ui, x, y, w, h, HIT_SLOT + slot);
}

// NatureName in the game's capitals, Hardy .. Quirky (personality % 25).
static const char* const NATURES[25] = {
    "HARDY", "LONELY", "BRAVE", "ADAMANT", "NAUGHTY", "BOLD", "DOCILE", "RELAXED", "IMPISH", "LAX",
    "TIMID", "HASTY", "SERIOUS", "JOLLY", "NAIVE", "MODEST", "MILD", "QUIET", "BASHFUL", "RASH",
    "CALM", "GENTLE", "SASSY", "CAREFUL", "QUIRKY",
};

// The struct's stat order is HP, ATTACK, DEFENSE, SPEED, SP. ATK, SP. DEF; the
// summary's is HP, ATTACK, DEFENSE, SP. ATK, SP. DEF, SPEED (SUMMARY_STAT_ORDER).
static const int SUMMARY_ORDER[6] = { 0, 1, 2, 4, 5, 3 };
static const char* const STAT_LABELS[6] = { "HP", "ATTACK", "DEFENSE", "SPEED", "SP. ATK", "SP. DEF" };

#define EXP_BLUE 0x40B8F8
#define STATS_PURPLE 0x9070D0
#define NATURE_UP 0xE04040
#define NATURE_DOWN 0x3984F7
#define IV_PERFECT 0xE8B830

// Hidden Power's types, Fighting first, as type ids (hiddenPowerTypes).
static const int HIDDEN_POWER_TYPES[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17 };

static int iv_bits(const int* ivs, int bit) {
    int v = 0;
    for (int i = 0; i < 6; i++) v |= ((ivs[i] >> bit) & 1) << i;
    return v;
}

// The name line every page starts with: name + gender, the species when
// nicknamed, Lv at the right. Returns the line's y + its height.
static int name_line(struct pd_canvas* c, int ix, int iw, int ty, const struct pd_mon* m) {
    char buf[40], buf2[40];
    const char* name = mon_display_name(m, buf, sizeof(buf));
    int nameW = pd_text(c, ix, ty, name, PD_LABEL, PD_LABEL_SHADOW);
    if (m->species != PD_SPECIES_EGG) {
        gender_mark(c, ix + nameW + 3, ty, m->gender);
        char lv[12];
        snprintf(lv, sizeof(lv), "Lv%d", m->level);
        pd_text_right(c, ix + iw, ty, lv, PD_VALUE, PD_VALUE_SHADOW);
        const char* species = pd_species_name(m->species, buf2, sizeof(buf2));
        if (strcmp(species, name)) pd_text(c, ix + nameW + 14, ty, species, PD_MUTED, PD_MUTED_SHADOW);
    }
    return ty + 18;
}

// HP, EXP (progress through this level, the text what's left to the next),
// the types and the held item, beside / under the icon.
static int info_block(struct pd_canvas* c, const struct pd_game* g, int ix0, int iw0, int ty, const struct pd_mon* m) {
    char buf[40];
    int ix = ix0 + PD_ICON + 4, iw = iw0 - PD_ICON - 4;
    bool fainted = m->hp == 0;
    if (pd_draw_mon_icon(c, g, m->species, ix0 - 2, ty - 16, fainted)) {
        ui_held_marker(c, g, ix0 - 2 + 16, ty - 16 + 22, m->heldItem);
    }
    ui_hp_bar(c, ix, ty + 4, 96, m->hp, m->maxHp);
    snprintf(buf, sizeof(buf), "%d/%d", m->hp, m->maxHp);
    pd_text(c, ix + 120, ty, buf, PD_LABEL, PD_LABEL_SHADOW);
    ui_status_badge(c, ix + iw - 30, ty + 2, m->status);
    ty += 16;

    int rate = m->species > 0 && m->species < pd_growth_rates_count ? pd_growth_rates[m->species] : -1;
    if (rate >= 0 && rate < 6 && m->level >= 1 && m->level <= 100) {
        const int* t = pd_exp_tables[rate];
        long start = t[m->level], next = m->level < 100 ? t[m->level + 1] : start;
        pd_text(c, ix, ty, "EXP", PD_VALUE, PD_VALUE_SHADOW);
        long span = next > start ? next - start : 1;
        long have = m->level < 100 ? (long) m->exp - start : span;
        if (have < 0) have = 0;
        ui_bar(c, ix + 24, ty + 5, 90, have, span, EXP_BLUE);
        if (m->level >= 100) snprintf(buf, sizeof(buf), "MAX");
        else snprintf(buf, sizeof(buf), "NEXT %ld", next > (long) m->exp ? next - (long) m->exp : 0);
        pd_text(c, ix + 120, ty, buf, PD_LABEL, PD_LABEL_SHADOW);
    }
    ty += 20;

    int t1, t2, cx = ix0;
    pd_species_type_pair(m->species, &t1, &t2);
    cx += ui_type_chip(c, cx, ty + 2, t1, NULL) + 4;
    if (t2 != t1) cx += ui_type_chip(c, cx, ty + 2, t2, NULL) + 4;
    if (m->heldItem) {
        int lx = ix0 + iw0 / 2;
        int lw = pd_text(c, lx, ty, "ITEM", PD_VALUE, PD_VALUE_SHADOW);
        pd_text_fit(c, lx + lw + 6, ty, ix0 + iw0 - lx - lw - 6, pd_item_name(g, m->heldItem), PD_LABEL, PD_LABEL_SHADOW);
    }
    return ty + 20;
}

static void moves_page(struct pd_canvas* c, int ix, int iw, int ty, const struct pd_mon* m) {
    char buf[40];
    for (int i = 0; i < PD_NUM_MOVES; i++) {
        if (!m->moves[i]) continue;
        pd_text_fit(c, ix, ty, 112, pd_move_name(m->moves[i], buf, sizeof(buf)), PD_LABEL, PD_LABEL_SHADOW);
        ui_type_chip(c, ix + 118, ty + 2, pd_move_type(m->moves[i]), NULL);
        int power = pd_move_power(m->moves[i]);
        if (power > 1) {
            snprintf(buf, sizeof(buf), "PWR %d", power);
            pd_text_right(c, ix + iw - 46, ty, buf, PD_MUTED, PD_MUTED_SHADOW);
        }
        snprintf(buf, sizeof(buf), "PP %d", m->pp[i]);
        pd_text_right(c, ix + iw, ty, buf, PD_VALUE, PD_VALUE_SHADOW);
        ty += 18;
    }
}

// MonStatsSection: per stat its value, IV (a 0-31 bar) and EV, the nature's
// raised / lowered stats marked; the totals; NATURE and HIDDEN POWER.
static void stats_page(struct pd_canvas* c, int ix, int iw, int ty, const struct pd_mon* m) {
    char buf[24];
    int up = m->nature / 5 != m->nature % 5 ? 1 + m->nature / 5 : -1;
    int down = m->nature / 5 != m->nature % 5 ? 1 + m->nature % 5 : -1;
    int statR = ix + 112, barX = ix + 122, barW = iw - 122 - 64, ivR = barX + barW + 24, evR = ix + iw;
    pd_text_right(c, statR, ty, "STAT", PD_MUTED, PD_MUTED_SHADOW);
    pd_text(c, barX, ty, "IV", PD_MUTED, PD_MUTED_SHADOW);
    pd_text_right(c, evR, ty, "EV", PD_MUTED, PD_MUTED_SHADOW);
    ty += 16;
    int ivSum = 0, evSum = 0;
    for (int k = 0; k < 6; k++) {
        int st = SUMMARY_ORDER[k];
        int lw = pd_text(c, ix, ty, STAT_LABELS[st], PD_LABEL, PD_LABEL_SHADOW);
        if (st == up) pd_triangle(c, ix + lw + 4, ty + 5, 5, PD_UP, NATURE_UP);
        if (st == down) pd_triangle(c, ix + lw + 4, ty + 5, 5, PD_DOWN, NATURE_DOWN);
        snprintf(buf, sizeof(buf), "%d", m->stats[st]);
        pd_text_right(c, statR, ty, buf, PD_LABEL, PD_LABEL_SHADOW);
        int iv = m->ivs[st];
        ui_bar(c, barX, ty + 5, barW, iv, 31, iv >= 31 ? IV_PERFECT : iv > 15 ? 0x73FFAD : iv > 6 ? 0xFFE739 : 0xFF7331);
        snprintf(buf, sizeof(buf), "%d", iv);
        pd_text_right(c, ivR, ty, buf, PD_VALUE, PD_VALUE_SHADOW);
        snprintf(buf, sizeof(buf), "%d", m->evs[st]);
        pd_text_right(c, evR, ty, buf, PD_VALUE, PD_VALUE_SHADOW);
        ivSum += iv;
        evSum += m->evs[st];
        ty += 14;
    }
    pd_text(c, ix, ty, "TOTAL", PD_LABEL, PD_LABEL_SHADOW);
    ui_bar(c, barX, ty + 5, barW, ivSum, 31 * 6, ivSum >= 186 ? IV_PERFECT : 0x73FFAD);
    snprintf(buf, sizeof(buf), "%d", ivSum);
    pd_text_right(c, ivR, ty, buf, PD_VALUE, PD_VALUE_SHADOW);
    snprintf(buf, sizeof(buf), "%d", evSum);
    pd_text_right(c, evR, ty, buf, PD_VALUE, PD_VALUE_SHADOW);
    ty += 18;
    pd_fill(c, ix, ty, iw, 1, PD_DIVIDER);
    ty += 4;
    int lw = pd_text(c, ix, ty, "NATURE", PD_LABEL, PD_LABEL_SHADOW);
    pd_text(c, ix + lw + 6, ty, NATURES[m->nature % 25], PD_VALUE, PD_VALUE_SHADOW);
    // Hidden Power: the type from the IVs' low bits, the power (30-70) from
    // their second bits - the Gen 3 rule, which FireRed / Emerald keep.
    int hx = ix + iw / 2;
    lw = pd_text(c, hx, ty, "H. POWER", PD_LABEL, PD_LABEL_SHADOW);
    int cw = ui_type_chip(c, hx + lw + 6, ty + 2, HIDDEN_POWER_TYPES[iv_bits(m->ivs, 0) * 15 / 63], NULL);
    snprintf(buf, sizeof(buf), "%d", iv_bits(m->ivs, 1) * 40 / 63 + 30);
    pd_text(c, hx + lw + 6 + cw + 6, ty, buf, PD_VALUE, PD_VALUE_SHADOW);
}

// The app's typeMatchups: every real attacking type (not ???) bucketed into
// WEAK TO (>1x), RESISTS (<1x) and IMMUNE TO (0x), as wrapping type chips.
static void types_page(struct pd_canvas* c, int ix, int iw, int ty, const struct pd_mon* m) {
    int t1, t2;
    pd_species_type_pair(m->species, &t1, &t2);
    if (t1 == PD_TYPE_NONE) {
        pd_text(c, ix, ty, "NO MATCHUP DATA", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    static const char* const HEADS[3] = { "WEAK TO", "RESISTS", "IMMUNE TO" };
    for (int b = 0; b < 3; b++) {
        bool any = false;
        int cx = ix;
        for (int atk = 0; atk < pd_type_names_count; atk++) {
            if (!pd_type_names[atk] || !strcmp(pd_type_names[atk], "???")) continue;
            int pct = pd_type_multiplier_pct(atk, t1, t2);
            int bucket = pct == 0 ? 2 : pct > 100 ? 0 : pct < 100 ? 1 : -1;
            if (bucket != b) continue;
            if (!any) {
                pd_text(c, ix, ty, HEADS[b], PD_VALUE, PD_VALUE_SHADOW);
                ty += 18;
                any = true;
            }
            const char* mult = pct == 400 ? "4x" : pct == 200 ? "2x" : pct == 50 ? "1/2x" : pct == 25 ? "1/4x" : "0x";
            char label[24];
            snprintf(label, sizeof(label), "%s %s", pd_type_name(atk), mult);
            int w = pd_text_width(label) + 8;
            if (cx + w > ix + iw) {
                cx = ix;
                ty += 17;
            }
            cx += ui_type_chip(c, cx, ty, atk, mult) + 4;
        }
        if (any) ty += 21;
    }
}

#define LEARN_GREEN 0x389830
#define FORGET_RED 0xD04030
#define VERDICT_GREY 0x8C8C94

// A pill of white text (VerdictPill); returns its width.
static int verdict_pill(struct pd_canvas* c, int x, int y, const char* text, uint32_t fill) {
    return ui_pill(c, x, y, text, fill);
}

// CanLearnSection: NEXT LEVELS, then TMs IN THE BAG - each its label, name and
// type, then LEARN / FORGET <move> / SKIP and its PWR / ACC / PP; the open one
// says why, in green (good for it) and red pills.
static void learn_page(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                       const struct pd_mon* m, int ix, int iw, int ty, int bottom) {
    static struct pd_learn_option opts[PD_LEARN_MAX];
    int n = pd_learn_options(g, s, m, opts, PD_LEARN_MAX);
    if (!n) {
        pd_text(c, ix, ty, "NOTHING TO LEARN FOR NOW", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    char buf[64];
    int h = bottom - ty;
    // Heights first: a heading, two lines a row, the open one's reasons.
    int total = 0;
    for (int i = 0; i < n; i++) {
        if (i == 0 || !opts[i].tm[0] != !opts[i - 1].tm[0]) total += 18;
        total += 36 + (i == ui->learnPick ? opts[i].advice.reasonCount * 16 : 0);
    }
    ui->learnScroll = ui_clamp_scroll(ui->learnScroll, total, h);
    ui_add_scroll_hit(ui, ix - 2, ty, iw + 4, h, HIT_LEARN_LIST);
    pd_clip(c, ix - 2, ty, iw + 4, h);
    int y = ty - ui->learnScroll;
    for (int i = 0; i < n; i++) {
        const struct pd_learn_option* o = &opts[i];
        if (i == 0 || !o->tm[0] != !opts[i - 1].tm[0]) {
            pd_text(c, ix, y, o->tm[0] ? "TMs IN THE BAG" : "NEXT LEVELS", PD_VALUE, PD_VALUE_SHADOW);
            y += 18;
        }
        bool open = i == ui->learnPick;
        int rowH = 34 + (open ? o->advice.reasonCount * 16 : 0);
        if (open) pd_round_rect(c, ix - 2, y - 1, iw + 4, rowH, 3, PD_LIST_FILL);
        int type = pd_move_type(o->move);
        pd_fill(c, ix - 2, y + 1, 3, rowH - 4, type < 18 && pd_type_colors[type] ? pd_type_colors[type] : PD_MUTED);
        if (o->tm[0]) snprintf(buf, sizeof(buf), "%s", o->tm);
        else snprintf(buf, sizeof(buf), "Lv %d", o->level);
        int lw = pd_text(c, ix + 6, y, buf, PD_MUTED, PD_MUTED_SHADOW);
        int cw = pd_text_width(pd_type_name(type)) + 8;
        char name[32];
        pd_text_fit(c, ix + 6 + lw + 6, y, iw - lw - 18 - cw, pd_move_name(o->move, name, sizeof(name)), PD_LABEL,
                    PD_LABEL_SHADOW);
        ui_type_chip(c, ix + iw - cw, y + 2, type, NULL);
        // VerdictBadge.
        const struct pd_advice* a = &o->advice;
        if (a->verdict == PD_REPLACE) snprintf(buf, sizeof(buf), "FORGET %s", pd_move_name(a->forget, name, sizeof(name)));
        else snprintf(buf, sizeof(buf), "%s", a->verdict == PD_LEARN ? "LEARN" : a->verdict == PD_KNOWN ? "LEARNED" : "SKIP");
        verdict_pill(c, ix + 6, y + 17, buf, a->verdict == PD_SKIP ? VERDICT_GREY : LEARN_GREEN);
        const struct pd_move_stats* st = o->move < pd_move_stats_count ? &pd_move_stats[o->move] : NULL;
        int power = pd_move_power(o->move);
        char pwr[12], acc[12];
        snprintf(pwr, sizeof(pwr), power > 1 ? "%d" : "-", power);
        snprintf(acc, sizeof(acc), st && st->accuracy ? "%d" : "-", st ? st->accuracy : 0);
        snprintf(buf, sizeof(buf), "PWR %s  ACC %s  PP %d", pwr, acc, st ? st->pp : 0);
        pd_text_right(c, ix + iw, y + 17, buf, PD_MUTED, PD_MUTED_SHADOW);
        for (int r = 0; open && r < a->reasonCount; r++) {
            verdict_pill(c, ix + 6, y + 34 + r * 16, a->reasons[r].text, a->reasons[r].good ? LEARN_GREEN : FORGET_RED);
        }
        ui_add_scroll_hit(ui, ix - 2, y, iw + 4, rowH, HIT_LEARN_ROW + i);
        y += rowH + 2;
    }
    pd_unclip(c);
    ui_scroll_bar(c, ix + iw, ty, h, ui->learnScroll, total, PD_MUTED);
}

static void page_button(struct pd_ui* ui, struct pd_canvas* c, int x, int y, int w, const char* label, int page) {
    if (ui->summaryPage == page) {
        // The page shown: STATS' purple, like the app's TextNavButton, no tap.
        pd_round_rect(c, x, y, w, 22, 3, STATS_PURPLE);
        pd_text(c, x + (w - pd_text_width(label)) / 2, pd_text_y(y, 22), label, 0xFFFFFF, PD_NO_SHADOW);
        return;
    }
    ui_button(ui, c, x, y, w, 22, label, HIT_SUMMARY_PAGE + page);
}

static void summary(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                    const struct pd_mon* m) {
    int x = MARGIN, y = CONTENT_Y, w = c->w - 2 * MARGIN, h = c->h - CONTENT_Y - MARGIN;
    int f = pd_title_box(c, x, y, w, h, PD_TITLE_FILL);
    int ix = x + f + 6, iw = w - 2 * (f + 6);
    int ty = y + f + 2;
    int by = y + h - f - 26;
    ui_button(ui, c, x + w - f - 70, by, 66, 22, "BACK", HIT_BACK);
    if (m->species == PD_SPECIES_EGG) {
        pd_draw_mon_icon(c, g, m->species, ix - 2, ty + 2, false);
        name_line(c, ix + PD_ICON + 2, iw - PD_ICON - 2, ty, m);
        pd_text(c, ix + PD_ICON + 2, ty + 20, "It's an egg. It will hatch in time.", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    if (ui->summaryPage == PD_SUMMARY_MOVES) {
        ty = name_line(c, ix + PD_ICON + 4, iw - PD_ICON - 4, ty, m);
        ty = info_block(c, g, ix, iw, ty, m);
        pd_fill(c, ix, ty, iw, 1, PD_DIVIDER);
        moves_page(c, ix, iw, ty + 4, m);
    } else {
        pd_draw_mon_icon(c, g, m->species, ix - 2, ty - 8, m->hp == 0);
        ty = name_line(c, ix + PD_ICON + 4, iw - PD_ICON - 4, ty, m);
        ty += 6;
        if (ui->summaryPage == PD_SUMMARY_STATS) stats_page(c, ix, iw, ty, m);
        else if (ui->summaryPage == PD_SUMMARY_TYPES) types_page(c, ix, iw, ty, m);
        else learn_page(ui, c, g, s, m, ix, iw, ty, by - 4);
    }
    static const char* const LABELS[PD_SUMMARY_PAGES] = { "MOVES", "STATS", "TYPES", "LEARN" };
    for (int p = 0; p < PD_SUMMARY_PAGES; p++) page_button(ui, c, ix - 2 + p * 56, by, 52, LABELS[p], p);
}

void ui_party_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    if (ui->summarySlot >= 0) {
        summary(ui, c, g, s, &s->party[ui->summarySlot]);
        return;
    }
    int gap = 4;
    int w = (c->w - 2 * MARGIN - gap) / 2;
    int h = (c->h - CONTENT_Y - MARGIN - 2 * gap) / 3;
    for (int i = 0; i < PD_PARTY_SIZE; i++) {
        int x = MARGIN + (i % 2) * (w + gap);
        int y = CONTENT_Y + (i / 2) * (h + gap);
        party_slot(ui, c, g, x, y, w, h, i < s->partyCount ? &s->party[i] : NULL, i);
    }
}
