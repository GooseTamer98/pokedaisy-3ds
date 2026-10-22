// The game actions' screens (the app's ItemPartyPicker, its move pick, the
// ItemNotice and MapScreen's FLY list): the party pick for USE / GIVE with the
// game's own verdict per Pokémon, a PP item's move pick, what an action did,
// and the towns FLY can go to. The buttons that ask for them sit on their tabs
// (BAG: USE / GIVE / PC, the summary: TAKE, MAP: FLY); the host runs each
// request between frames (pd_act_step) and hands back the answer
// (pd_ui_act_done).
#include <stdio.h>
#include <string.h>

#include "pd_icon.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

#define TITLE_H 24
#define SLOT_ROW 27
#define FLY_ROW 18

// A title window with a CANCEL button at its right end.
static void title(struct pd_ui* ui, struct pd_canvas* c, int y, const char* text) {
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int f = pd_title_box(c, x, y, w, TITLE_H, PD_TITLE_FILL);
    pd_text_fit(c, x + f + 6, pd_text_y(y, TITLE_H), w - 2 * f - 84, text, PD_TITLE_TEXT, PD_TITLE_SHADOW);
    ui_button(ui, c, x + w - f - 72, y + 3, 68, TITLE_H - 6, "CANCEL", HIT_NO);
}

static const char* verdict_text(const struct pd_target* t, bool give, const struct pd_game* g) {
    switch (t->verdict) {
    case PD_SLOT_NO_EFFECT: return "NO EFFECT";
    case PD_SLOT_EGG: return "EGG";
    case PD_SLOT_HOLDS_MAIL: return "HOLDS MAIL";
    case PD_SLOT_HOLDS_IT: return "HOLDS IT";
    case PD_SLOT_BAG_FULL: return "BAG FULL";
    default: return give && t->heldItem ? pd_item_name(g, t->heldItem) : "";
    }
}

static void party_pick(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    const struct pd_act* a = &ui->act;
    char buf[64];
    snprintf(buf, sizeof(buf), ui->actGive ? "Give %s to which POKéMON?" : "Use %s on which POKéMON?",
             pd_item_name(g, a->item));
    int x = MARGIN, w = c->w - 2 * MARGIN, y = CONTENT_Y;
    title(ui, c, y, buf);
    y += TITLE_H + 4;
    int n = a->targetCount < s->partyCount ? a->targetCount : s->partyCount;
    int lf = pd_list_box(c, x, y, w, c->h - y - MARGIN);
    int ix = x + lf, iw = w - 2 * lf, ry = y + lf + 1;
    pd_clip(c, ix, ry, iw, c->h - MARGIN - lf - ry);
    for (int i = 0; i < n; i++, ry += SLOT_ROW) {
        const struct pd_mon* m = &s->party[i];
        const struct pd_target* t = &a->targets[i];
        bool ok = t->verdict == PD_SLOT_OK;
        if (ok && ui->pressedId == HIT_ACT_SLOT + i) pd_fill(c, ix, ry, iw, SLOT_ROW, PD_ROW_SELECTED);
        pd_draw_mon_icon(c, g, m->species, ix + 2, ry - 6, !ok);
        uint32_t col = ok ? PD_LABEL : PD_MUTED, sh = ok ? PD_LABEL_SHADOW : PD_MUTED_SHADOW;
        pd_text_fit(c, ix + 36, ry - 1, 92, m->nickname, col, sh);
        if (m->species != PD_SPECIES_EGG) {
            snprintf(buf, sizeof(buf), "Lv%d", m->level);
            pd_text(c, ix + 36, ry + 11, buf, col, sh);
            ui_hp_bar(c, ix + 132, ry + 4, 64, m->hp, m->maxHp);
            snprintf(buf, sizeof(buf), "%d/%d", m->hp, m->maxHp);
            pd_text(c, ix + 140, ry + 11, buf, col, sh);
        }
        const char* v = verdict_text(t, ui->actGive, g);
        if (*v) pd_text_right(c, ix + iw - 6, pd_text_y(ry, SLOT_ROW), v, PD_MUTED, PD_MUTED_SHADOW);
        if (ok) ui_add_hit(ui, ix, ry, iw, SLOT_ROW, HIT_ACT_SLOT + i);
        pd_fill(c, ix + 4, ry + SLOT_ROW - 1, iw - 8, 1, PD_DIVIDER);
    }
    pd_unclip(c);
}

static void move_pick(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    const struct pd_act* a = &ui->act;
    if (a->slot < 0 || a->slot >= s->partyCount) return;
    const struct pd_mon* m = &s->party[a->slot];
    int x = MARGIN, w = c->w - 2 * MARGIN, y = CONTENT_Y;
    title(ui, c, y, pd_act_pp_boost(a->item) ? "Boost PP of which move?" : "Restore PP of which move?");
    y += TITLE_H + 4;
    int lf = pd_list_box(c, x, y, w, 4 * 26 + 2 * 7 + 4);
    int ix = x + lf, iw = w - 2 * lf, ry = y + lf + 2;
    for (int i = 0; i < PD_NUM_MOVES; i++, ry += 26) {
        if (!m->moves[i]) continue;
        bool ok = a->targets[a->slot].moves[i];
        if (ok && ui->pressedId == HIT_ACT_MOVE + i) pd_fill(c, ix, ry, iw, 26, PD_ROW_SELECTED);
        char name[32], pp[16];
        int type = pd_move_type(m->moves[i]);
        int cw = ui_type_chip(c, ix + 8, ry + 6, type, NULL);
        pd_text_fit(c, ix + 14 + cw, pd_text_y(ry, 26), iw - cw - 90, pd_move_name(m->moves[i], name, sizeof(name)),
                    ok ? PD_LABEL : PD_MUTED, ok ? PD_LABEL_SHADOW : PD_MUTED_SHADOW);
        snprintf(pp, sizeof(pp), "PP %d", m->pp[i]);
        pd_text_right(c, ix + iw - 8, pd_text_y(ry, 26), pp, ok ? PD_LABEL : PD_MUTED, ok ? PD_LABEL_SHADOW : PD_MUTED_SHADOW);
        if (ok) ui_add_hit(ui, ix, ry, iw, 26, HIT_ACT_MOVE + i);
    }
    (void) g;
}

static void notice(struct pd_ui* ui, struct pd_canvas* c) {
    int x = MARGIN, w = c->w - 2 * MARGIN, y = CONTENT_Y + 30;
    int h = 3 * 16 + 2 * 7 + 6;
    int lf = pd_list_box(c, x, y, w, h);
    pd_text_wrap(c, x + lf + 6, y + lf + 2, w - 2 * (lf + 6), 16, 3, ui->actNotice, PD_LABEL, PD_LABEL_SHADOW);
    ui_button(ui, c, c->w / 2 - 48, y + h + 6, 96, 24, "OK", HIT_ACT_OK);
}

static void fly_list(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g) {
    const struct pd_act* a = &ui->act;
    int x = MARGIN, w = c->w - 2 * MARGIN, y = CONTENT_Y;
    title(ui, c, y, "FLY to where?");
    y += TITLE_H + 4;
    int lf = pd_list_box(c, x, y, w, c->h - y - MARGIN);
    int ix = x + lf, iy = y + lf + 1, iw = w - 2 * lf, ih = c->h - y - MARGIN - 2 * lf - 2;
    ui_add_scroll_hit(ui, ix, iy, iw, ih, HIT_FLY_LIST);
    ui->flyScroll = ui_clamp_scroll(ui->flyScroll, a->flyCount * FLY_ROW, ih);
    pd_clip(c, ix, iy, iw, ih);
    for (int i = ui->flyScroll / FLY_ROW; i < a->flyCount; i++) {
        int ry = iy + i * FLY_ROW - ui->flyScroll;
        if (ry >= iy + ih) break;
        if (ui->pressedId == HIT_FLY_TOWN + i) pd_fill(c, ix, ry, iw, FLY_ROW, PD_ROW_SELECTED);
        pd_text_fit(c, ix + 14, pd_text_y(ry, FLY_ROW), iw - 28, pd_mapsec_name(g, a->fly[i]), PD_LABEL, PD_LABEL_SHADOW);
        int top = ry < iy ? iy : ry;
        int bottom = ry + FLY_ROW > iy + ih ? iy + ih : ry + FLY_ROW;
        ui_add_scroll_hit(ui, ix, top, iw, bottom - top, HIT_FLY_TOWN + i);
    }
    pd_unclip(c);
    ui_scroll_bar(c, ix + iw - 4, iy, ih, ui->flyScroll, a->flyCount * FLY_ROW, PD_MUTED);
}

void ui_act_overlay(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    pd_dim(c, 0xE6);
    ui_add_hit(ui, 0, 0, c->w, c->h, HIT_SCRIM);
    switch (ui->overlay) {
    case PD_OVERLAY_ACT_PARTY: party_pick(ui, c, g, s); break;
    case PD_OVERLAY_ACT_MOVE: move_pick(ui, c, g, s); break;
    case PD_OVERLAY_ACT_NOTICE: notice(ui, c); break;
    case PD_OVERLAY_FLY: fly_list(ui, c, g); break;
    default: break;
    }
}

bool ui_act_act(struct pd_ui* ui, int id) {
    struct pd_act* a = &ui->act;
    const struct pd_snapshot* s = ui->snap;
    if (ui->overlay >= PD_OVERLAY_ACT_PARTY && ui->overlay <= PD_OVERLAY_FLY) {
        if (id == HIT_NO || id == HIT_SCRIM || id == HIT_ACT_OK) {
            ui->overlay = PD_OVERLAY_NONE;
        } else if (id >= HIT_ACT_SLOT && id < HIT_ACT_SLOT + PD_PARTY_SIZE && ui->overlay == PD_OVERLAY_ACT_PARTY) {
            a->slot = id - HIT_ACT_SLOT;
            a->personality = a->targets[a->slot].personality;
            if (!ui->actGive && ui->game && pd_act_picks_move(ui->game, a->item)) {
                ui->overlay = PD_OVERLAY_ACT_MOVE;
            } else {
                ui->overlay = PD_OVERLAY_NONE;
                pd_act_request(a, ui->actGive ? PD_REQ_GIVE_TO : PD_REQ_USE_ON);
            }
        } else if (id >= HIT_ACT_MOVE && id < HIT_ACT_MOVE + PD_NUM_MOVES && ui->overlay == PD_OVERLAY_ACT_MOVE) {
            a->move = id - HIT_ACT_MOVE;
            ui->overlay = PD_OVERLAY_NONE;
            pd_act_request(a, PD_REQ_USE_ON);
        } else if (id >= HIT_FLY_TOWN && id < HIT_FLY_TOWN + a->flyCount && ui->overlay == PD_OVERLAY_FLY) {
            a->mapsec = a->fly[id - HIT_FLY_TOWN];
            ui->overlay = PD_OVERLAY_NONE;
            pd_act_request(a, PD_REQ_FLY);
        }
        return true; // the overlay takes every tap
    }
    switch (id) {
    case HIT_ACT_USE:
    case HIT_ACT_GIVE:
        if (ui->bagSelected <= 0) return true;
        a->item = ui->bagSelected;
        a->slot = -1;
        ui->actGive = id == HIT_ACT_GIVE;
        pd_act_request(a, ui->actGive ? PD_REQ_GIVE_TARGETS : PD_REQ_USE);
        return true;
    case HIT_ACT_TAKE:
        if (!s || ui->summarySlot < 0 || ui->summarySlot >= s->partyCount) return true;
        a->slot = ui->summarySlot;
        a->personality = s->party[a->slot].personality;
        pd_act_request(a, PD_REQ_TAKE);
        return true;
    case HIT_ACT_PC:
        a->slot = -1;
        pd_act_request(a, PD_REQ_PC);
        return true;
    case HIT_ACT_FLY:
        a->slot = -1;
        ui->flyScroll = 0;
        pd_act_request(a, PD_REQ_FLY_TARGETS);
        return true;
    default:
        return false;
    }
}

void pd_ui_act_done(struct pd_ui* ui, const struct pd_game* g, const struct pd_snapshot* s) {
    struct pd_act* a = &ui->act;
    enum pd_act_kind done = a->done;
    a->done = PD_REQ_NONE;
    if (a->result == PD_ACT_USED) {
        if ((done == PD_REQ_USE && pd_act_picks_mon(g, a->item)) || done == PD_REQ_GIVE_TARGETS) {
            ui->overlay = PD_OVERLAY_ACT_PARTY;
            return;
        }
        if (done == PD_REQ_FLY_TARGETS) {
            if (a->flyCount) {
                ui->overlay = PD_OVERLAY_FLY;
                return;
            }
            snprintf(ui->actNotice, sizeof(ui->actNotice), "There's no town to FLY to yet.");
            ui->overlay = PD_OVERLAY_ACT_NOTICE;
            return;
        }
    }
    const char* text = pd_act_message(a, g, s, ui->actNotice, sizeof(ui->actNotice));
    if (*text) {
        if (text != ui->actNotice) snprintf(ui->actNotice, sizeof(ui->actNotice), "%s", text);
        ui->overlay = PD_OVERLAY_ACT_NOTICE;
    } else if (ui->overlay >= PD_OVERLAY_ACT_PARTY && ui->overlay <= PD_OVERLAY_FLY) {
        ui->overlay = PD_OVERLAY_NONE;
    }
}
