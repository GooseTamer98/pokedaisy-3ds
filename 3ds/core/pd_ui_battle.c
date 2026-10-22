// BATTLE: CONTROLS (BattleControlsScreen.kt), INFO (BattleInfoScreen.kt),
// SUGGESTIONS (SuggestionsScreen.kt) and FOE DETAILS.
//
// CONTROLS, where the game's battle menu can be driven (pd_bctl.c): INFO and
// SUGGESTIONS over a big FIGHT and BAG / RUN / POKéMON; FIGHT opens the move
// grid (each move with its verdict), POKéMON the party (OUT, BEST), and a
// pick presses the game's own menu. While the game's bag or party screen is
// up, BACK; otherwise it waits for the player's turn.
//
// INFO: one white window split by rules - the foe, you, then your moves with
// their plain-words verdicts against the foe. Your card carries the app's
// ThreatPill (what the foe's own types do to you); a tap on the foe's card
// opens FOE DETAILS (the app's expand button): its WEAK TO / RESISTS /
// IMMUNE TO, ability, held item, stats with their stages, and its moves with
// what each does to you. SUGGESTIONS: the foe on a VS
// line, then the party's three best picks against it, each with its best
// move (the app's rankSuggestions). Under either, a bar: the button to the
// other pane and, in a trainer battle, the FOE TEAM - the trainer's party in
// order, Poké Balls until sent out or announced, fainted ones faded, the one
// the panes are about framed in red. Tapping one shows it (NEXT / FAINTED /
// RESERVE / UNSEEN), tapping it again follows the battle.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pd_dex.h"
#include "pd_icon.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

#define BAR_H 34
#define ROW 34      // a card's row: the icon and two lines of text
#define RULE 5      // a separator and the space around it
#define FRAME 3     // the white window's frame
#define LINE 16     // PD_FONT_HEIGHT

// BattleControlsScreen's button colours.
#define SUGGEST_GOLD 0xE8B830
#define SUGGEST_GOLD_DARK 0xA87C10
#define INFO_PURPLE 0x9068D0
#define INFO_PURPLE_DARK 0x5C3C98
#define VERDICT_GREY 0x8C8C94
#define OUT_GREEN 0x48A858
// verdictColor: a hit that lands well / badly, from the side it's about.
#define VERDICT_GOOD 0x389830
#define VERDICT_BAD 0xD04030

// What a card shows: a battler (types from the battle) or a party Pokémon.
// FOE DETAILS' facts: ability (0 = not read: a reserve foe's isn't), held
// item, ATTACK / DEFENSE / SPEED / SP. ATK / SP. DEF and, for a battler, their
// stages.
struct mon_view {
    int species, level, hp, maxHp;
    int type1, type2;
    uint32_t status;
    const int* moves;
    const int* pp;
    int ability, item;
    const int* stats;
    const int* stages;
};

static struct mon_view from_battler(const struct pd_battle_mon* b) {
    return (struct mon_view) { b->species, b->level, b->hp, b->maxHp, b->type1, b->type2, b->status, b->moves, b->pp,
                               b->ability, b->item, b->stats, b->stages };
}

static struct mon_view from_party(const struct pd_mon* m) {
    struct mon_view v = { m->species, m->level, m->hp, m->maxHp, PD_TYPE_NONE, PD_TYPE_NONE, m->status, m->moves, m->pp,
                          0, m->heldItem, m->stats + 1, NULL };
    pd_species_type_pair(m->species, &v.type1, &v.type2);
    return v;
}

static void types_text(int t1, int t2, char* buf, size_t len) {
    if (t1 == PD_TYPE_NONE) snprintf(buf, len, "-");
    else if (t2 != t1 && t2 != PD_TYPE_NONE) snprintf(buf, len, "%s/%s", pd_type_name(t1), pd_type_name(t2));
    else snprintf(buf, len, "%s", pd_type_name(t1));
}

// A small pill of white text (VerdictPill): returns its width.
static int pill(struct pd_canvas* c, int x, int y, const char* text, uint32_t fill) {
    int w = pd_text_width(text) + 6;
    pd_round_rect(c, x, y + 1, w, 14, 2, fill);
    pd_text(c, x + 3, y - 1, text, 0xFFFFFF, PD_NO_SHADOW);
    return w;
}

// The icon on the window's grey, or a ball when the ROM has no icon for it.
static void icon(struct pd_canvas* c, const struct pd_game* g, int species, int x, int y, bool faded) {
    pd_round_rect(c, x, y, PD_ICON + 2, PD_ICON + 2, 3, PD_LIST_FILL);
    if (!pd_draw_mon_icon(c, g, species, x + 1, y + 1, faded)) ui_ball(c, x + 12, y + 12, true);
}

// A card across w: icon, then [side] name ... [state] Lv on the first line,
// types, HP bar and HP on the second.
static void card(struct pd_canvas* c, const struct pd_game* g, int x, int y, int w, const struct mon_view* m,
                 const char* side, const char* state, uint32_t stateFill) {
    char buf[40];
    icon(c, g, m->species, x, y, false);
    int tx = x + PD_ICON + 8, right = x + w;
    int ty = y + 1;
    int nx = tx;
    if (side) nx += pd_text(c, nx, ty, side, PD_VALUE, PD_VALUE_SHADOW) + 5;
    char lv[12];
    snprintf(lv, sizeof(lv), "Lv%d", m->level);
    int lvx = right - pd_text_width(lv);
    pd_text(c, lvx, ty, lv, PD_LABEL, PD_LABEL_SHADOW);
    int stateW = state && *state ? pd_text_width(state) + 6 + 4 : 0;
    const char* status = pd_status_label(m->status);
    int statusW = *status ? pd_text_width(status) + 6 + 4 : 0;
    int nameEnd = nx + pd_text_fit(c, nx, ty, lvx - 6 - stateW - statusW - nx,
                                   pd_species_name(m->species, buf, sizeof(buf)), PD_LABEL, PD_LABEL_SHADOW);
    if (*status) {
        ui_status_badge(c, nameEnd + 4, ty + 1, m->status);
        nameEnd += statusW;
    }
    if (stateW) pill(c, nameEnd + 4, ty, state, stateFill);

    ty += LINE + 1;
    types_text(m->type1, m->type2, buf, sizeof(buf));
    int typesW = pd_text_fit(c, tx, ty, 92, buf, PD_MUTED, PD_MUTED_SHADOW);
    char hp[16];
    snprintf(hp, sizeof(hp), "%d/%d", m->hp, m->maxHp);
    int hpx = right - pd_text_width(hp);
    pd_text(c, hpx, ty, hp, PD_LABEL, PD_LABEL_SHADOW);
    int barX = tx + (typesW > 92 ? typesW : 92) + 6;
    int barW = hpx - 6 - barX - 18;
    if (barW >= 24) ui_hp_bar(c, barX, ty + 5, barW, m->hp, m->maxHp);
}

// The plain-words verdict of BattleInfoScreen: SUPER 2x / RESISTED ½x / ...
static void verdict(int pct, const char** text, uint32_t* color, uint32_t* shadow) {
    if (pct == 0) { *text = "IMMUNE 0x"; *color = PD_MUTED; *shadow = PD_MUTED_SHADOW; }
    else if (pct >= 400) { *text = "SUPER 4x"; *color = PD_VALUE; *shadow = PD_VALUE_SHADOW; }
    else if (pct >= 200) { *text = "SUPER 2x"; *color = PD_VALUE; *shadow = PD_VALUE_SHADOW; }
    else if (pct <= 25) { *text = "RESISTED 1/4x"; *color = PD_MUTED; *shadow = PD_MUTED_SHADOW; }
    else if (pct < 100) { *text = "RESISTED 1/2x"; *color = PD_MUTED; *shadow = PD_MUTED_SHADOW; }
    else { *text = "NEUTRAL 1x"; *color = PD_LABEL; *shadow = PD_LABEL_SHADOW; }
}

// A move on the window's grey: its name and PP, then its verdict on foe
// (NULL = no foe: no verdict). threat: the foe's move on you, so a resisted
// one is the good news (threatColor).
static void move_cell(struct pd_canvas* c, int x, int y, int w, int h, int move, int pp, const struct mon_view* foe,
                      bool threat) {
    char buf[40];
    pd_round_rect(c, x, y, w, h, 3, PD_LIST_FILL);
    if (!move) return;
    int ix = x + 5, iw = w - 10;
    int ty = y + (h - 2 * LINE - 1) / 2;
    char ppText[12];
    snprintf(ppText, sizeof(ppText), "PP %d", pp);
    int ppW = pd_text_width(ppText);
    pd_text_fit(c, ix, ty, iw - ppW - 6, pd_move_name(move, buf, sizeof(buf)), PD_LABEL, PD_LABEL_SHADOW);
    pd_text_right(c, ix + iw, ty, ppText, PD_MUTED, PD_MUTED_SHADOW);
    const char* text = "";
    uint32_t color = PD_LABEL, shadow = PD_LABEL_SHADOW;
    if (pd_move_power(move) == 0) {
        text = "STATUS";
        color = PD_MUTED;
        shadow = PD_MUTED_SHADOW;
    } else if (foe) {
        int pct = pd_type_multiplier_pct(pd_move_type(move), foe->type1, foe->type2);
        verdict(pct, &text, &color, &shadow);
        if (threat && pct < 100) {
            color = VERDICT_GOOD;
            shadow = PD_MUTED_SHADOW;
        }
    }
    pd_text(c, ix, ty + LINE + 1, text, color, shadow);
}

static void rule(struct pd_canvas* c, int x, int y, int w) {
    pd_fill(c, x, y + RULE / 2, w, 1, PD_DIVIDER);
}

// A coloured square-cornered button (PlatinumButton / TextNavButton).
static void nav_button(struct pd_ui* ui, struct pd_canvas* c, int x, int y, int w, int h, const char* label,
                       uint32_t fill, uint32_t dark, int id) {
    bool pressed = ui->pressedId == id;
    static const struct pd_layer FRAME_LAYERS[1] = { { 0, 2 } };
    struct pd_layer frame = FRAME_LAYERS[0];
    frame.color = dark;
    pd_layered_box(c, x, y, w, h, 3, &frame, 1, pressed ? dark : fill);
    pd_text(c, x + (w - pd_text_width(label)) / 2, pd_text_y(y, h), label, 0xFFFFFF, dark);
    ui_add_hit(ui, x, y, w, h, id);
}

// The FOE TEAM, from x to the right edge.
static void foe_strip(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                      int x, int y, int right) {
    if (s->foeCount < 2) return;
    int cell = BAR_H, gap = 3;
    int need = s->foeCount * cell + (s->foeCount - 1) * gap;
    if (right - x < need) cell = (right - x - (s->foeCount - 1) * gap) / s->foeCount;
    int sx = right - (s->foeCount * cell + (s->foeCount - 1) * gap);
    for (int i = 0; i < s->foeCount; i++, sx += cell + gap) {
        bool shown = (ui->foeSeen >> i & 1) || i == ui->foeShown;
        bool current = i == ui->foeShown;
        struct pd_layer layers[2] = { { PD_VALUE, 2 }, { PD_VALUE_SHADOW, 1 } };
        struct pd_layer plain[2] = { { 0x63737B, 2 }, { 0xCED6D6, 1 } };
        pd_layered_box(c, sx, y, cell, cell, 3, current ? layers : plain, 2, shown ? PD_TITLE_FILL : PD_LIST_FILL);
        const struct pd_mon* m = &s->foes[i];
        if (!shown) {
            ui_ball(c, sx + (cell - 10) / 2, y + (cell - 10) / 2, true);
        } else if (!pd_draw_mon_icon(c, g, m->species, sx + (cell - PD_ICON) / 2, y + (cell - PD_ICON) / 2,
                                     m->hp == 0)) {
            char lv[8];
            snprintf(lv, sizeof(lv), "%d", m->level);
            pd_text(c, sx + (cell - pd_text_width(lv)) / 2, pd_text_y(y, cell), lv,
                    m->hp ? PD_LABEL : PD_MUTED, PD_NO_SHADOW);
        }
        ui_add_hit(ui, sx, y, cell, cell, HIT_FOE_SLOT + i);
    }
}

// The foe the panes are about: the tapped one, else the one about to come
// in, else the one out (CompanionScreen's shownFoe). A party Pokémon only
// when it isn't the one out - that one is the battler, with its live types.
static const struct pd_mon* shown_foe(struct pd_ui* ui, const struct pd_snapshot* s, const char** state) {
    int shown = ui->foeSelected >= 0 ? ui->foeSelected : s->foeNext >= 0 ? s->foeNext : s->foeActive;
    ui->foeShown = shown;
    *state = "";
    if (shown < 0 || shown >= s->foeCount || shown == s->foeActive) return NULL;
    const struct pd_mon* m = &s->foes[shown];
    if (shown == s->foeNext) *state = "NEXT";
    else if (m->hp == 0) *state = "FAINTED";
    else if (ui->foeSeen >> shown & 1) *state = "RESERVE";
    else *state = "UNSEEN";
    return m;
}

// --- SUGGESTIONS ---

struct pick {
    const struct pd_mon* mon;
    int move, pp, pct, score;
};

// Whether party Pokémon m is one of yours on the field (isOut: battlers
// carry no personality to match on).
static bool is_out(const struct pd_mon* m, const struct pd_snapshot* s) {
    for (int p = 0; p < 4; p += 2) {
        const struct pd_battle_mon* b = &s->battlers[p];
        if (b->species && b->species == m->species && b->level == m->level && b->maxHp == m->maxHp) return true;
    }
    return false;
}

static int by_score(const void* a, const void* b) {
    const struct pick *x = a, *y = b;
    if (x->score != y->score) return y->score - x->score;
    return (int) (x->mon - y->mon); // stable: party order on ties
}

// rankSuggestions: every living party Pokémon with a usable damaging move,
// by (its best move's power x multiplier on the foe) minus the worst the
// foe's own types do to it.
static int rank(const struct pd_snapshot* s, const struct mon_view* foe, struct pick* out) {
    int n = 0;
    for (int i = 0; i < s->partyCount; i++) {
        const struct pd_mon* m = &s->party[i];
        if (m->hp <= 0 || m->species == PD_SPECIES_EGG) continue;
        int best = -1, bestValue = -1;
        for (int k = 0; k < PD_NUM_MOVES; k++) {
            int power = m->moves[k] ? pd_move_power(m->moves[k]) : 0;
            if (power <= 0 || m->pp[k] <= 0) continue;
            int value = power * pd_type_multiplier_pct(pd_move_type(m->moves[k]), foe->type1, foe->type2);
            if (value > bestValue) {
                bestValue = value;
                best = k;
            }
        }
        if (best < 0) continue;
        int t1, t2;
        pd_species_type_pair(m->species, &t1, &t2);
        int risk = 100;
        if (foe->type1 != PD_TYPE_NONE) {
            risk = pd_type_multiplier_pct(foe->type1, t1, t2);
            if (foe->type2 != PD_TYPE_NONE && foe->type2 != foe->type1) {
                int r2 = pd_type_multiplier_pct(foe->type2, t1, t2);
                if (r2 > risk) risk = r2;
            }
        }
        int pct = pd_type_multiplier_pct(pd_move_type(m->moves[best]), foe->type1, foe->type2);
        out[n++] = (struct pick) { m, m->moves[best], m->pp[best], pct, pd_move_power(m->moves[best]) * pct / 100 - risk };
    }
    qsort(out, (size_t) n, sizeof(out[0]), by_score);
    return n;
}

static void suggestions(struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                        const struct mon_view* foe, const char* foeState, int ix, int iy, int iw, int ih) {
    if (!foe) {
        pd_text(c, ix, iy, "NO FOE ON THE FIELD", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    card(c, g, ix, iy, iw, foe, "VS", foeState, VERDICT_GREY);
    int y = iy + ROW;
    rule(c, ix, y, iw);
    y += RULE;
    struct pick picks[PD_PARTY_SIZE];
    int n = rank(s, foe, picks);
    if (!n) {
        pd_text_wrap(c, ix, y + 2, iw, LINE, 2, "NO USABLE DAMAGING MOVES IN YOUR PARTY", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    static const char* const RANKS[3] = { "1ST", "2ND", "3RD" };
    int rowH = (iy + ih - y - 2 * RULE) / 3;
    int half = (iw - RULE) / 2;
    for (int i = 0; i < n && i < 3; i++) {
        if (i) {
            rule(c, ix, y, iw);
            y += RULE;
        }
        const struct pick* p = &picks[i];
        int ry = y + (rowH - ROW) / 2;
        // The Pokémon: rank and name, then its level, HP and OUT when it's in battle.
        char buf[40];
        icon(c, g, p->mon->species, ix, ry, false);
        int tx = ix + PD_ICON + 8, right = ix + half;
        int nx = tx + pd_text(c, tx, ry + 1, RANKS[i], PD_VALUE, PD_VALUE_SHADOW) + 5;
        pd_text_fit(c, nx, ry + 1, right - nx, pd_species_name(p->mon->species, buf, sizeof(buf)), PD_LABEL,
                    PD_LABEL_SHADOW);
        int ty = ry + LINE + 1;
        char lv[12];
        snprintf(lv, sizeof(lv), "Lv%d", p->mon->level);
        int bx = tx + pd_text(c, tx, ty, lv, PD_LABEL, PD_LABEL_SHADOW) + 5;
        int outW = is_out(p->mon, s) ? pd_text_width("OUT") + 6 : 0;
        if (outW) pill(c, right - outW, ty, "OUT", OUT_GREEN);
        int barW = right - (outW ? outW + 5 : 0) - bx - 18;
        if (barW > 48) barW = 48;
        if (barW >= 16) ui_hp_bar(c, bx, ty + 5, barW, p->mon->hp, p->mon->maxHp);
        // Its best move against the foe.
        pd_fill(c, ix + half + RULE / 2, y + 2, 1, rowH - 4, PD_DIVIDER);
        move_cell(c, ix + half + RULE, y + 1, iw - half - RULE, rowH - 2, p->move, p->pp, foe, false);
        y += rowH;
    }
}

// --- INFO ---

// ThreatPill: the worst the foe's own types do to you - "ROCK 2x" in red when
// one lands super effectively, RESISTS in green when every one is resisted.
static const char* threat_pill(const struct mon_view* foe, const struct mon_view* you, char* buf, size_t len,
                               uint32_t* fill) {
    if (!foe || foe->type1 == PD_TYPE_NONE || you->type1 == PD_TYPE_NONE) return "";
    int worst = -1, worstType = foe->type1;
    for (int k = 0; k < 2; k++) {
        int t = k ? foe->type2 : foe->type1;
        if (t == PD_TYPE_NONE || (k && t == foe->type1)) continue;
        int pct = pd_type_multiplier_pct(t, you->type1, you->type2);
        if (pct > worst) {
            worst = pct;
            worstType = t;
        }
    }
    if (worst > 100) {
        snprintf(buf, len, "%s %s", pd_type_name(worstType), worst >= 400 ? "4x" : "2x");
        *fill = VERDICT_BAD;
        return buf;
    }
    if (worst >= 0 && worst < 100) {
        *fill = VERDICT_GOOD;
        return "RESISTS";
    }
    return "";
}

static void info(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                 const struct mon_view* foe, const char* foeState, const struct mon_view* you, int ix, int iy,
                 int iw, int ih) {
    int y = iy;
    if (foe) {
        // The foe's card opens FOE DETAILS: a cursor at its right says so.
        card(c, g, ix, y, iw - 10, foe, s->isTrainer ? "FOE" : "WILD", foeState, VERDICT_GREY);
        pd_triangle(c, ix + iw - 5, y + ROW / 2 - 4, 5, PD_RIGHT, PD_MUTED);
        ui_add_hit(ui, ix, y, iw, ROW, HIT_BATTLE_FOE);
    } else {
        pd_text(c, ix, y + 1, "NO FOE ON THE FIELD", PD_MUTED, PD_MUTED_SHADOW);
    }
    y += ROW;
    rule(c, ix, y, iw);
    y += RULE;
    char threat[24];
    uint32_t threatFill = VERDICT_GREY;
    card(c, g, ix, y, iw, you, "YOU", threat_pill(foe, you, threat, sizeof(threat), &threatFill), threatFill);
    y += ROW;
    rule(c, ix, y, iw);
    y += RULE;
    int gap = 4;
    int bw = (iw - gap) / 2, bh = (iy + ih - y - gap) / 2;
    for (int i = 0; i < PD_NUM_MOVES; i++) {
        move_cell(c, ix + (i % 2) * (bw + gap), y + (i / 2) * (bh + gap), bw, bh, you->moves[i], you->pp[i], foe, false);
    }
}

// --- FOE DETAILS ---

// "+2" on the foe is bad news for you (red), "-1" good (green): StagePill.
static int stage_pill(struct pd_canvas* c, int x, int y, int stage) {
    char buf[16];
    snprintf(buf, sizeof(buf), stage > 0 ? "+%d" : "%d", stage);
    return pill(c, x, y, buf, stage > 0 ? VERDICT_BAD : VERDICT_GOOD);
}

// FoeDetailsColumn (expanded): what to hit it with (strongest first) and what
// not to, its ability and held item (FoeFacts), its stats with their stages,
// then its moves with what each does to you.
static void foe_details(struct pd_canvas* c, const struct pd_game* g, const struct mon_view* foe,
                        const struct mon_view* you, int ix, int iy, int iw, int ih) {
    char buf[40], name[24];
    if (!foe) {
        pd_text(c, ix, iy + 1, "NO FOE ON THE FIELD", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    int y = iy;
    icon(c, g, foe->species, ix, y, foe->hp == 0);
    int tx = ix + PD_ICON + 8;
    snprintf(buf, sizeof(buf), "%s IS", pd_species_name(foe->species, name, sizeof(name)));
    pd_text(c, tx, y + 1, buf, PD_VALUE, PD_VALUE_SHADOW);
    snprintf(buf, sizeof(buf), "Lv%d", foe->level);
    pd_text_right(c, ix + iw, y + 1, buf, PD_LABEL, PD_LABEL_SHADOW);

    // WEAK TO (strongest first), RESISTS (weakest first), IMMUNE TO: a line a
    // group, the heading on the line of its chips (MatchupGroup inline).
    int ly = y + LINE + 2;
    if (foe->type1 == PD_TYPE_NONE) {
        pd_text(c, tx, ly, "NO MATCHUP DATA", PD_MUTED, PD_MUTED_SHADOW);
        ly += 17;
    }
    static const char* const HEADS[3] = { "WEAK", "RESISTS", "IMMUNE" };
    static const int ORDER[3][4] = { { 400, 200, -1, -1 }, { 25, 50, -1, -1 }, { 0, -1, -1, -1 } };
    // Lines start beside the icon while there's icon beside them.
#define LINE_X(ly) ((ly) + 16 <= y + ROW ? tx : ix)
    for (int b = 0; b < 3 && foe->type1 != PD_TYPE_NONE; b++) {
        int cx = -1;
        for (int o = 0; o < 4 && ORDER[b][o] >= 0; o++) {
            for (int atk = 0; atk < pd_type_names_count; atk++) {
                if (!pd_type_names[atk] || !strcmp(pd_type_names[atk], "???")) continue;
                if (pd_type_multiplier_pct(atk, foe->type1, foe->type2) != ORDER[b][o]) continue;
                const char* mult = ORDER[b][o] == 400 ? "4x" : ORDER[b][o] == 200 ? "2x"
                    : ORDER[b][o] == 50 ? "1/2x" : ORDER[b][o] == 25 ? "1/4x" : "0x";
                if (cx < 0) cx = LINE_X(ly) + pd_text(c, LINE_X(ly), ly, HEADS[b], PD_LABEL, PD_LABEL_SHADOW) + 6;
                snprintf(buf, sizeof(buf), "%s %s", pd_type_name(atk), mult);
                if (cx + pd_text_width(buf) + 8 > ix + iw) {
                    ly += 16;
                    cx = LINE_X(ly);
                }
                cx += ui_type_chip(c, cx, ly + 2, atk, mult) + 3;
            }
        }
        if (cx >= 0) ly += 16;
    }
#undef LINE_X
    y = ly > y + ROW ? ly : y + ROW;
    rule(c, ix, y, iw);
    y += RULE;

    // FoeFacts: ABILITY and HELD ITEM, then the five stats with their stages.
    int half = iw / 2;
    // A reserve foe's ability isn't read (the app leaves the line out too).
    int lw;
    if (*pd_ability_name(g, foe->ability, buf, sizeof(buf))) {
        lw = pd_text(c, ix, y, "ABILITY", PD_LABEL, PD_LABEL_SHADOW);
        pd_text_fit(c, ix + lw + 5, y, half - lw - 10, buf, PD_VALUE, PD_VALUE_SHADOW);
    }
    lw = pd_text(c, ix + half, y, "HELD ITEM", PD_LABEL, PD_LABEL_SHADOW);
    pd_text_fit(c, ix + half + lw + 5, y, half - lw - 5, foe->item ? pd_item_name(g, foe->item) : "NONE", PD_VALUE,
                PD_VALUE_SHADOW);
    y += LINE + 1;
    if (foe->stats) {
        // The summary's order: ATTACK, DEFENSE, SP. ATK, SP. DEF, SPEED.
        static const int SORDER[5] = { 0, 1, 3, 4, 2 };
        static const char* const SLABEL[5] = { "ATK", "DEF", "SPEED", "SP.ATK", "SP.DEF" };
        int colW = iw / 5;
        for (int k = 0; k < 5; k++) {
            int i = SORDER[k], cx = ix + k * colW;
            pd_text(c, cx, y, SLABEL[i], PD_MUTED, PD_MUTED_SHADOW);
            snprintf(buf, sizeof(buf), "%d", foe->stats[i]);
            int vw = pd_text(c, cx, y + LINE - 1, buf, PD_VALUE, PD_VALUE_SHADOW);
            if (foe->stages && foe->stages[i]) stage_pill(c, cx + vw + 3, y + LINE - 1, foe->stages[i]);
        }
        y += 2 * LINE;
        // Accuracy / evasion, only when changed.
        if (foe->stages && (foe->stages[5] || foe->stages[6])) {
            int cx = ix;
            for (int i = 5; i < 7; i++) {
                if (!foe->stages[i]) continue;
                cx += pd_text(c, cx, y, i == 5 ? "ACCURACY" : "EVASION", PD_LABEL, PD_LABEL_SHADOW) + 4;
                cx += stage_pill(c, cx, y, foe->stages[i]) + 10;
            }
            y += LINE;
        }
    }
    rule(c, ix, y, iw);
    y += RULE;

    // Its moves, two by two, each with what it does to you.
    bool any = false;
    for (int i = 0; i < PD_NUM_MOVES; i++) any |= foe->moves[i] != 0;
    if (!any) {
        pd_text(c, ix, y, "NO MOVE DATA", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    // Two by two (the expanded pane's shortVerdict): name, then what it does
    // to you - the multiplier, or STATUS.
    int colW = (iw - 8) / 2;
    int rowH = (iy + ih - y) / 2;
    if (rowH > LINE + 2) rowH = LINE + 2;
    for (int i = 0; i < PD_NUM_MOVES; i++) {
        int move = foe->moves[i];
        if (!move) continue;
        int mx = ix + (i % 2) * (colW + 8), my = y + (i / 2) * rowH;
        const char* text = "STATUS";
        uint32_t color = PD_MUTED, shadow = PD_MUTED_SHADOW;
        if (pd_move_power(move) > 0) {
            int pct = pd_type_multiplier_pct(pd_move_type(move), you->type1, you->type2);
            const char* full;
            verdict(pct, &full, &color, &shadow);
            text = pct == 0 ? "0x" : pct >= 400 ? "4x" : pct >= 200 ? "2x" : pct <= 25 ? "1/4x" : pct < 100 ? "1/2x" : "1x";
            if (pct < 100) color = VERDICT_GOOD;
        }
        int tw = pd_text_width(text);
        pd_text_fit(c, mx, my, colW - tw - 6, pd_move_name(move, buf, sizeof(buf)), PD_LABEL, PD_LABEL_SHADOW);
        pd_text_right(c, mx + colW, my, text, color, shadow);
    }
}

// --- CONTROLS (BattleControlsScreen) -------------------------------------------

#define FIGHT_RED 0xE8483C
#define FIGHT_RED_DARK 0xA82820
#define BAG_ORANGE 0xE0A030
#define BAG_ORANGE_DARK 0xA87018
#define RUN_BLUE 0x4890D8
#define RUN_BLUE_DARK 0x2860A0
#define MON_GREEN 0x58A840
#define MON_GREEN_DARK 0x307020
#define DISABLED_GREY 0xB0B0B8
#define DISABLED_DARK 0x80808C

// A button that presses the game's menu: greyed while a pick is under way.
static void game_button(struct pd_ui* ui, struct pd_canvas* c, int x, int y, int w, int h, const char* label,
                        uint32_t fill, uint32_t dark, int id, bool enabled) {
    if (enabled) {
        nav_button(ui, c, x, y, w, h, label, fill, dark, id);
        return;
    }
    struct pd_layer frame = { DISABLED_DARK, 2 };
    pd_layered_box(c, x, y, w, h, 3, &frame, 1, DISABLED_GREY);
    pd_text(c, x + (w - pd_text_width(label)) / 2, pd_text_y(y, h), label, 0xFFFFFF, DISABLED_DARK);
}

static void centred_note(struct pd_canvas* c, int ix, int iy, int iw, int ih, const char* text) {
    pd_text(c, ix + (iw - pd_text_width(text)) / 2, iy + ih / 2 - LINE / 2, text, PD_MUTED, PD_MUTED_SHADOW);
}

// The party pane (PartyPicker): the game's party in its order, OUT on the one
// in battle, BEST on SUGGESTIONS' top pick; a tap sends that one in.
static void party_pane(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                       const struct mon_view* foe, int ix, int iy, int iw, int ih, bool enabled) {
    struct pick picks[PD_PARTY_SIZE];
    int n = foe ? rank(s, foe, picks) : 0;
    const struct pd_mon* best = NULL;
    for (int i = 0; i < n && !best; i++) {
        if (!is_out(picks[i].mon, s)) best = picks[i].mon;
    }
    int rowH = (ih - 26) / 6;
    if (rowH > 26) rowH = 26;
    pd_text(c, ix, iy, "Choose a POKéMON.", PD_LABEL, PD_LABEL_SHADOW);
    int y = iy + LINE + 2;
    char buf[24];
    for (int i = 0; i < s->partyCount; i++, y += rowH) {
        const struct pd_mon* m = &s->party[i];
        bool out = is_out(m, s), egg = m->species == PD_SPECIES_EGG;
        bool ok = enabled && !out && !egg && m->hp > 0;
        if (ok && ui->pressedId == HIT_BC_SLOT + i) pd_fill(c, ix - 2, y, iw + 4, rowH, PD_ROW_SELECTED);
        pd_clip(c, ix - 2, y, iw + 4, rowH);
        pd_draw_mon_icon(c, g, m->species, ix - 2, y + rowH / 2 - 22, m->hp == 0 || egg);
        pd_unclip(c);
        int ty = pd_text_y(y, rowH);
        uint32_t col = ok || out ? PD_LABEL : PD_MUTED, sh = ok || out ? PD_LABEL_SHADOW : PD_MUTED_SHADOW;
        pd_text_fit(c, ix + 32, ty, 86, m->nickname, col, sh);
        if (!egg) {
            snprintf(buf, sizeof(buf), "Lv%d", m->level);
            pd_text(c, ix + 122, ty, buf, col, sh);
            ui_hp_bar(c, ix + 160, y + rowH / 2 - 3, 60, m->hp, m->maxHp);
        }
        if (out) pill(c, ix + iw - 34, ty, "OUT", OUT_GREEN);
        else if (m == best) pill(c, ix + iw - 38, ty, "BEST", VERDICT_GOOD);
        else if (m->hp == 0 && !egg) pill(c, ix + iw - 50, ty, "FAINTED", VERDICT_GREY);
        if (ok) ui_add_hit(ui, ix - 2, y, iw + 4, rowH, HIT_BC_SLOT + i);
    }
}

static void controls(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                     const struct mon_view* foe, const struct mon_view* you, int ix, int iy, int iw, int ih) {
    bool enabled = !pd_bctl_busy(&ui->bctl);
    int state = s->inputState;
    // Without gPartyMenu (Ruby / Sapphire: their party cursor is a sprite)
    // there's no switch of ours: POKéMON opens the game's own party screen.
    bool picker = g->cfg && g->cfg->partyMenu;
    if (state != PD_BIN_ACTION && state != PD_BIN_MOVE) ui->battleChoose = state == PD_BIN_PARTY && picker ? 2 : 0;
    // The party pane takes the window's whole height (the app's).
    if ((state == PD_BIN_PARTY && picker) || (state == PD_BIN_ACTION && ui->battleChoose == 2)) {
        party_pane(ui, c, g, s, foe, ix, iy, iw, ih - 24, enabled);
        if (state == PD_BIN_PARTY) game_button(ui, c, ix + iw - 80, iy + ih - 22, 80, 22, "BACK", RUN_BLUE, RUN_BLUE_DARK, HIT_BC_BACK, enabled);
        else game_button(ui, c, ix + iw - 80, iy + ih - 22, 80, 22, "CANCEL", RUN_BLUE, RUN_BLUE_DARK, HIT_BC_CANCEL, true);
        return;
    }
    // INFO / SUGGESTIONS over the menu: our own panes, never the game's.
    int tw = 70, sw = pd_text_width("SUGGESTIONS") + 16;
    int tx = ix + (iw - tw - sw - 8) / 2;
    nav_button(ui, c, tx, iy, tw, 22, "INFO", INFO_PURPLE, INFO_PURPLE_DARK, HIT_BATTLE_INFO);
    nav_button(ui, c, tx + tw + 8, iy, sw, 22, "SUGGESTIONS", SUGGEST_GOLD, SUGGEST_GOLD_DARK, HIT_BATTLE_SUGGEST);
    int y = iy + 28, h = ih - 28;
    if (state == PD_BIN_MOVE || (state == PD_BIN_ACTION && ui->battleChoose == 1)) {
        // The move grid: each with its verdict on the foe; CANCEL closes ours,
        // or backs out of the game's move screen.
        int cw = (iw - 6) / 2, chh = (h - 30) / 2;
        for (int i = 0; i < PD_NUM_MOVES; i++) {
            int cx = ix + (i % 2) * (cw + 6), cy = y + (i / 2) * (chh + 4);
            int move = you->moves[i];
            bool ok = enabled && move && you->pp[i] > 0;
            move_cell(c, cx, cy, cw, chh, move, you->pp[i], foe, false);
            if (move) {
                int type = pd_move_type(move);
                uint32_t frame = type < 18 && pd_type_colors[type] ? pd_type_colors[type] : PD_MUTED;
                pd_fill(c, cx, cy + 2, 3, chh - 4, frame);
            }
            if (ui->pressedId == HIT_BC_MOVE + i && ok) pd_dim(c, 0x40);
            if (ok) ui_add_hit(ui, cx, cy, cw, chh, HIT_BC_MOVE + i);
        }
        game_button(ui, c, ix + iw - 80, y + h - 22, 80, 22, "CANCEL", RUN_BLUE, RUN_BLUE_DARK,
                    state == PD_BIN_MOVE ? HIT_BC_BACK : HIT_BC_CANCEL, enabled || state != PD_BIN_MOVE);
        return;
    }
    if (state == PD_BIN_BAG || state == PD_BIN_PARTY) {
        centred_note(c, ix, y, iw, h - 30, state == PD_BIN_BAG ? "The game's bag is open." : "The game's party screen is open.");
        game_button(ui, c, ix + (iw - 80) / 2, y + h - 26, 80, 24, "BACK", RUN_BLUE, RUN_BLUE_DARK, HIT_BC_BACK, enabled);
        return;
    }
    if (state != PD_BIN_ACTION) {
        centred_note(c, ix, y, iw, h, state == PD_BIN_TARGET ? "Confirming target..." : "Waiting for your turn...");
        return;
    }
    // Platinum's layout: a big FIGHT up top with the battler's icon, BAG and
    // POKéMON in the bottom corners, RUN between them a little lower.
    int fw = iw * 76 / 100, fh = h * 52 / 100;
    int fx = ix + (iw - fw) / 2;
    game_button(ui, c, fx, y, fw, fh, "", FIGHT_RED, FIGHT_RED_DARK, HIT_BC_FIGHT, enabled);
    int lw = pd_text_width("FIGHT");
    int gx = fx + (fw - lw - PD_ICON - 6) / 2;
    pd_draw_mon_icon(c, g, you->species, gx, y + fh / 2 - 20, false);
    pd_text(c, gx + PD_ICON + 6, pd_text_y(y, fh), "FIGHT", 0xFFFFFF, enabled ? FIGHT_RED_DARK : DISABLED_DARK);
    int by = y + fh + 8, bh = h - fh - 8 - 8, bw = (iw - 16) / 3;
    game_button(ui, c, ix, by, bw, bh, "BAG", BAG_ORANGE, BAG_ORANGE_DARK, HIT_BC_FIGHT + 1, enabled);
    game_button(ui, c, ix + bw + 8, by + 8, bw, bh, "RUN", RUN_BLUE, RUN_BLUE_DARK, HIT_BC_FIGHT + 3,
                enabled && !s->isTrainer);
    game_button(ui, c, ix + 2 * (bw + 8), by, bw, bh, "POKéMON", MON_GREEN, MON_GREEN_DARK, HIT_BC_FIGHT + 2, enabled);
}

void ui_battle_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    const struct pd_battle_mon* youB = &s->battlers[PD_POS_PLAYER_LEFT];
    if (!s->inBattle || !youB->species) {
        ui_notice(c, "NO BATTLE", "A battle opens this tab by itself,", "and its end takes you back.");
        return;
    }
    // The foe: the FOE TEAM's shown one, else the battler out.
    const char* foeState;
    const struct pd_mon* party = shown_foe(ui, s, &foeState);
    struct mon_view foeView, you = from_battler(youB);
    const struct mon_view* foe = NULL;
    if (party) {
        foeView = from_party(party);
        foe = &foeView;
    } else if (s->battlers[PD_POS_OPPONENT_LEFT].species) {
        foeView = from_battler(&s->battlers[PD_POS_OPPONENT_LEFT]);
        foe = &foeView;
    }

    int x = MARGIN, w = c->w - 2 * MARGIN;
    int barY = c->h - MARGIN - BAR_H;
    int h = barY - 4 - CONTENT_Y;
    int f = pd_title_box(c, x, CONTENT_Y, w, h, PD_TITLE_FILL);
    int ix = x + f + 4, iy = CONTENT_Y + f + 2, iw = w - 2 * (f + 4), ih = h - 2 * (f + 2);
    bool controlsKnown = pd_battle_input_known(g);
    if (ui->battlePane == PD_BATTLE_CONTROLS && !controlsKnown) ui->battlePane = 0;
    if (ui->battlePane == PD_BATTLE_CONTROLS) controls(ui, c, g, s, foe, &you, ix, iy, iw, ih);
    else if (ui->battlePane == 1) suggestions(c, g, s, foe, foeState, ix, iy, iw, ih);
    else if (ui->battlePane == 2) foe_details(c, g, foe, &you, ix, iy, iw, ih);
    else info(ui, c, g, s, foe, foeState, &you, ix, iy, iw, ih);

    // The bar: BACK to the controls (where the game has them), the other
    // pane's button, then the FOE TEAM. One width for INFO / SUGGESTIONS, so
    // the same spot flips back.
    int bx = x;
    if (controlsKnown && ui->battlePane != PD_BATTLE_CONTROLS) {
        nav_button(ui, c, bx, barY, 50, BAR_H, "BACK", RUN_BLUE, RUN_BLUE_DARK, HIT_BC_CONTROLS);
        bx += 50 + 6;
    }
    if (ui->battlePane != PD_BATTLE_CONTROLS) {
        const char* label = ui->battlePane ? "INFO" : "SUGGESTIONS";
        int bw = pd_text_width("SUGGESTIONS") + 20;
        if (ui->battlePane) nav_button(ui, c, bx, barY, bw, BAR_H, label, INFO_PURPLE, INFO_PURPLE_DARK, HIT_BATTLE_INFO);
        else nav_button(ui, c, bx, barY, bw, BAR_H, label, SUGGEST_GOLD, SUGGEST_GOLD_DARK, HIT_BATTLE_SUGGEST);
        bx += bw + 8;
    }
    foe_strip(ui, c, g, s, bx, barY, x + w);
}

bool ui_battle_act(struct pd_ui* ui, int id) {
    const struct pd_snapshot* s = ui->snap;
    const struct pd_game* g = ui->game;
    if (id == HIT_BC_CONTROLS) {
        ui->battlePane = PD_BATTLE_CONTROLS;
        return true;
    }
    if (id == HIT_BC_CANCEL) {
        ui->battleChoose = 0;
        return true;
    }
    if (id == HIT_BC_BACK) {
        pd_bctl_back(&ui->bctl);
        ui->battleChoose = 0;
        return true;
    }
    if (id >= HIT_BC_FIGHT && id < HIT_BC_FIGHT + 4) {
        int action = id - HIT_BC_FIGHT;
        // FIGHT and POKéMON open our own grid / party first (the app's): the
        // game's screen only opens once a move or a Pokémon is picked.
        if (action == 0) ui->battleChoose = 1;
        else if (action == 2 && g && g->cfg && g->cfg->partyMenu) ui->battleChoose = 2;
        else pd_bctl_action(&ui->bctl, action);
        return true;
    }
    if (id >= HIT_BC_MOVE && id < HIT_BC_MOVE + PD_NUM_MOVES) {
        pd_bctl_move(&ui->bctl, id - HIT_BC_MOVE);
        ui->battleChoose = 0;
        return true;
    }
    if (id >= HIT_BC_SLOT && id < HIT_BC_SLOT + PD_PARTY_SIZE) {
        int slot = id - HIT_BC_SLOT;
        if (s && slot < s->partyCount) pd_bctl_switch(&ui->bctl, s->party[slot].personality);
        ui->battleChoose = 0;
        return true;
    }
    if (id == HIT_BATTLE_SUGGEST || id == HIT_BATTLE_INFO || id == HIT_BATTLE_FOE) {
        ui->battlePane = id == HIT_BATTLE_SUGGEST ? 1 : id == HIT_BATTLE_FOE ? 2 : 0;
        return true;
    }
    if (id >= HIT_FOE_SLOT && id < HIT_FOE_SLOT + PD_PARTY_SIZE) {
        int i = id - HIT_FOE_SLOT;
        // Tapping the one shown goes back to following the battle.
        ui->foeSelected = i == ui->foeShown && ui->foeSelected >= 0 ? -1 : i;
        return true;
    }
    return false;
}
