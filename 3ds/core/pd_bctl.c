#include "pd_bctl.h"

#include <string.h>

#define KEY_A 0x001
#define KEY_B 0x002
#define KEY_UP 0x040
#define KEY_DOWN 0x080

#define MAX_BATTLERS 4
#define WATCHDOG 1200      // frames any pick may take
#define RETRY_A 20         // frames an A may go unanswered before it's pressed again
#define TARGET_WATCH 300   // frames after a move to watch for its target screen
#define RUN_PRESSES 8      // B presses through RUN's messages, every RUN_GAP frames
#define RUN_GAP 30
#define MON_SIZE 100

static uint32_t u32(const uint8_t* p) {
    return (uint32_t) p[0] | (uint32_t) p[1] << 8 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24;
}

bool pd_battle_input_known(const struct pd_game* g) {
    return g->cfg && g->cfg->battlerControllerFuncs && g->cfg->inputAction && g->cfg->inputMove;
}

// readNativeBattleInputFast: the first battler whose controller function is
// one of the player's handlers (the Thumb bit masked on both sides).
enum pd_bin pd_battle_input(const struct pd_game* g, pd_read_fn read, void* ctx, int* battler) {
    const struct pd_config* cfg = g->cfg;
    *battler = 0;
    if (!pd_battle_input_known(g)) return PD_BIN_NONE;
    uint8_t flags = 0, raw[MAX_BATTLERS * 4];
    if (!read(ctx, pd_in_battle_addr(cfg), &flags, 1) || !(flags & 2)) return PD_BIN_NONE;
    if (!read(ctx, cfg->battlerControllerFuncs, raw, sizeof(raw))) return PD_BIN_NONE;
    for (int i = 0; i < MAX_BATTLERS; i++) {
        uint32_t fn = u32(raw + i * 4) & ~1u;
        enum pd_bin s = fn == (cfg->inputAction & ~1u) ? PD_BIN_ACTION
            : fn == (cfg->inputMove & ~1u) ? PD_BIN_MOVE
            : cfg->inputItem && fn == (cfg->inputItem & ~1u) ? PD_BIN_BAG
            : cfg->inputParty && fn == (cfg->inputParty & ~1u) ? PD_BIN_PARTY
            : cfg->inputTarget && fn == (cfg->inputTarget & ~1u) ? PD_BIN_TARGET : PD_BIN_NONE;
        if (s != PD_BIN_NONE) {
            *battler = i;
            return s;
        }
    }
    return PD_BIN_BUSY;
}

static void start(struct pd_bctl* b, enum pd_bc_goal goal, int cell) {
    b->goal = goal;
    b->cell = cell;
    b->stage = 0;
    b->frames = b->waited = b->presses = 0;
    b->hold = b->gap = 0;
    b->targetWatch = b->runWatch = 0;
}

void pd_bctl_action(struct pd_bctl* b, int action) { start(b, PD_BC_ACTION, action); }
void pd_bctl_move(struct pd_bctl* b, int move) { start(b, PD_BC_MOVE, move); }

void pd_bctl_switch(struct pd_bctl* b, uint32_t personality) {
    start(b, PD_BC_SWITCH, 2);
    b->personality = personality;
}

void pd_bctl_back(struct pd_bctl* b) {
    start(b, PD_BC_NONE, 0);
    b->keys = KEY_B;
    b->hold = 2;
    b->gap = 2;
}

bool pd_bctl_busy(const struct pd_bctl* b) {
    return b->goal != PD_BC_NONE || b->hold > 0;
}

static void press(struct pd_bctl* b, uint16_t keys, int gap) {
    b->keys = keys;
    b->hold = 2;
    b->gap = gap;
    b->presses++;
}

static void done(struct pd_bctl* b) {
    b->goal = PD_BC_NONE;
}

// Puts the menu's cursor on cell (the game reads it on A) and reads it back.
static bool write_cell(const struct pd_game_io* io, uint32_t cursors, int battler, int cell) {
    if (!cursors) return false;
    uint8_t v = (uint8_t) cell, back = 0xFF;
    uint32_t at = cursors + (uint32_t) battler;
    return io->write(io->ctx, at, &v, 1) && io->read(io->ctx, at, &back, 1) && back == v;
}

// Without a writable cursor, the app's blind walk: LEFT, UP, then RIGHT /
// DOWN to the cell (column + 2 x row), each settling, then A.
static void walk(struct pd_bctl* b, int cell) {
    static const uint16_t STEPS[4] = { 0x020, 0x040, 0x010, 0x080 }; // LEFT UP RIGHT DOWN
    int n = b->waited++;
    if (n == 0) press(b, STEPS[0], 20);
    else if (n == 1) press(b, STEPS[1], 20);
    else if (n == 2 && (cell & 1)) press(b, STEPS[2], 20);
    else if (n <= 3 && (cell & 2)) {
        b->waited = 4;
        press(b, STEPS[3], 20);
    } else {
        b->waited = 0;
        press(b, KEY_A, 4);
        b->stage++;
    }
}

// The action menu's pick: the cell, A. Stage advances once the game takes it.
static void pick_action(struct pd_bctl* b, const struct pd_game* g, const struct pd_game_io* io, int cell) {
    if (b->state == PD_BIN_MOVE) {
        press(b, KEY_B, 6); // back to the action menu
        return;
    }
    if (b->state != PD_BIN_ACTION) return;
    if (write_cell(io, g->cfg->actionCursor, b->battler, cell)) {
        press(b, KEY_A, 4);
        b->stage++;
        b->waited = 0;
    } else {
        walk(b, cell);
    }
}

// Waits for the menu to give way after an A; presses it again if dropped.
static bool left_menu(struct pd_bctl* b, enum pd_bin menu) {
    if (b->state != menu) return true;
    if (++b->waited >= RETRY_A) {
        b->waited = 0;
        b->stage--;
    }
    return false;
}

// The party slot holding personality (gPlayerParty, battle order), -1 = gone.
static int slot_of(const struct pd_game* g, const struct pd_game_io* io, uint32_t personality) {
    uint8_t count = 0;
    io->read(io->ctx, g->cfg->playerPartyCount, &count, 1);
    for (int i = 0; i < count && i < 6; i++) {
        uint8_t p[4];
        if (io->read(io->ctx, g->cfg->playerParty + (uint32_t) i * MON_SIZE, p, 4) && u32(p) == personality) return i;
    }
    return -1;
}

static void step_switch(struct pd_bctl* b, const struct pd_game* g, const struct pd_game_io* io) {
    switch (b->stage) {
    case 0: // POKéMON on the action menu (a faint opens the party screen by itself)
        if (b->state == PD_BIN_PARTY) b->stage = 2;
        else pick_action(b, g, io, 2);
        return;
    case 1:
        if (b->state == PD_BIN_PARTY) b->stage = 2;
        else if (b->state == PD_BIN_ACTION) left_menu(b, PD_BIN_ACTION);
        return;
    case 2: { // steer the party menu's cursor to the Pokémon
        if (b->state != PD_BIN_PARTY || !g->cfg->partyMenu) return;
        int want = slot_of(g, io, b->personality);
        if (want < 0) {
            done(b);
            return;
        }
        uint8_t slot = 0xFF;
        io->read(io->ctx, g->cfg->partyMenu + 9, &slot, 1);
        if (slot == want) {
            press(b, KEY_A, 8);
            b->stage = 3;
        } else {
            // The shorter way; a press the menu dropped (its fade) is pressed again.
            press(b, slot > want && slot < 6 ? KEY_UP : KEY_DOWN, 3);
        }
        return;
    }
    case 3: // SHIFT / SEND OUT: the first choice of the slot's menu
        press(b, KEY_A, 6);
        b->stage = 4;
        b->waited = 0;
        return;
    case 4: // the party screen closes; a refusal ("can't be switched") leaves it open
        if (b->state != PD_BIN_PARTY) {
            done(b);
        } else if (++b->waited > 90) {
            press(b, KEY_B, 6);
            done(b);
        }
        return;
    }
}

uint16_t pd_bctl_frame(struct pd_bctl* b, const struct pd_game* g, const struct pd_game_io* io, uint16_t player) {
    if (b->hold > 0) {
        b->hold--;
        return b->keys;
    }
    bool steering = b->goal != PD_BC_NONE;
    if (b->gap > 0) {
        b->gap--;
        return steering ? 0 : player;
    }
    b->state = pd_battle_input(g, io->read, io->ctx, &b->battler);
    if (b->state == PD_BIN_NONE) {
        start(b, PD_BC_NONE, 0);
        return player;
    }
    // The passive watches: they add a press, never take the player's keys away.
    if (!steering) {
        if (b->targetWatch > 0) {
            b->targetWatch--;
            // A move that targets "user or selected" opens the target screen in a
            // single battle too: its A accepts the game's own default.
            if (b->state == PD_BIN_TARGET) {
                b->targetWatch = 0;
                press(b, KEY_A, 2);
                return player | KEY_A;
            }
        }
        if (b->runWatch > 0) {
            if (b->state == PD_BIN_ACTION) {
                b->runWatch = 0; // "Can't escape!": the menu is back
            } else if (--b->runGap <= 0) {
                b->runWatch--;
                b->runGap = RUN_GAP;
                press(b, KEY_B, 2);
                return player | KEY_B;
            }
        }
        return player;
    }
    if (++b->frames > WATCHDOG) {
        done(b);
        return player;
    }
    switch (b->goal) {
    case PD_BC_ACTION:
        if (b->stage == 0) {
            pick_action(b, g, io, b->cell);
        } else if (left_menu(b, PD_BIN_ACTION)) {
            if (b->cell == 3) {
                b->runWatch = RUN_PRESSES;
                b->runGap = RUN_GAP;
            }
            done(b);
        }
        break;
    case PD_BC_MOVE:
        if (b->stage == 0) {
            if (b->state == PD_BIN_MOVE) b->stage = 2;
            else pick_action(b, g, io, 0);
        } else if (b->stage == 1) {
            if (b->state == PD_BIN_MOVE) {
                b->stage = 2;
                b->waited = 0;
            } else {
                left_menu(b, PD_BIN_ACTION);
            }
        } else if (b->stage == 2) {
            if (b->state != PD_BIN_MOVE) break;
            if (write_cell(io, g->cfg->moveCursor, b->battler, b->cell)) {
                press(b, KEY_A, 4);
                b->stage = 3;
                b->waited = 0;
            } else {
                // The blind walk, then A.
                b->stage = 2;
                walk(b, b->cell);
                if (b->stage == 3) b->waited = 0;
            }
        } else if (left_menu(b, PD_BIN_MOVE)) {
            b->targetWatch = TARGET_WATCH;
            done(b);
        }
        break;
    case PD_BC_SWITCH:
        step_switch(b, g, io);
        break;
    default:
        break;
    }
    return b->hold > 0 ? b->keys : 0;
}
