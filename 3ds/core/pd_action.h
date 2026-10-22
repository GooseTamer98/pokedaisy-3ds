// The game's own functions, called between frames - the app's FieldItems.kt,
// Pc.kt and Fly.kt: USE (a Repel; medicine and PP items on a Pokémon), GIVE,
// TAKE, the PC anywhere and FLY. Nothing steers the game's menus: each is the
// call the menu would end in (VarSet / RemoveBagItem, ExecuteTableBasedItemEffect,
// SetMonData, a PC script, SetFlyWarpDestination ...), only where the ROM's code
// is found byte for byte where these tables say (retail English FireRed rev 1,
// LeafGreen rev 0 / 1 and Emerald, from pret's decomp builds), and only on the
// player's free field.
//
// The host reaches the game through pd_game_io; the companion asks for a
// request (pd_act_request) and the host runs it between frames (pd_act_step).
#ifndef PD_ACTION_H
#define PD_ACTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pd_game.h"
#include "pd_snapshot.h"

struct pd_game_io {
    void* ctx;
    bool (*read)(void* ctx, uint32_t addr, void* out, size_t len);
    // RAM only (EWRAM / IWRAM).
    bool (*write)(void* ctx, uint32_t addr, const void* in, size_t len);
    // Runs Thumb function fn with r0-r3 = args to its return; false when it
    // can't run now (the game is mid-frame in its own code) or never returned.
    bool (*call)(void* ctx, uint32_t fn, const uint32_t args[4], uint32_t* r0);
};

enum pd_act_result {
    PD_ACT_NONE,
    PD_ACT_USED,
    PD_ACT_STILL_ACTIVE, // a Repel is still working
    PD_ACT_NOT_NOW,      // a script, a message, a menu
    PD_ACT_IN_BATTLE,
    PD_ACT_NONE_LEFT,
    PD_ACT_FAILED,
    PD_ACT_NO_EFFECT,    // the game's "It won't have any effect."
    PD_ACT_GIVEN,
    PD_ACT_BAG_FULL,
    PD_ACT_PARTY_CHANGED,
    PD_ACT_TAKEN,
    PD_ACT_NO_BADGE,
    PD_ACT_CANT_FLY_HERE,
    PD_ACT_NO_FLYER,
    PD_ACT_FLYING,
};

// What a party slot is to a USE / GIVE / TAKE (the app's SlotVerdict).
enum pd_slot_verdict { PD_SLOT_OK, PD_SLOT_NO_EFFECT, PD_SLOT_EGG, PD_SLOT_HOLDS_MAIL, PD_SLOT_HOLDS_IT, PD_SLOT_BAG_FULL };

struct pd_target {
    uint32_t personality; // checked again before acting
    int heldItem;
    enum pd_slot_verdict verdict;
    bool moves[4]; // a one-move PP item: which moves it would help
};

enum pd_act_kind {
    PD_REQ_NONE,
    PD_REQ_USE,          // a Repel at once; else the party pick (targets)
    PD_REQ_GIVE_TARGETS, // the party pick for GIVE
    PD_REQ_USE_ON,       // item on slot (move)
    PD_REQ_GIVE_TO,      // item to slot
    PD_REQ_TAKE,         // slot's held item into the bag
    PD_REQ_PC,
    PD_REQ_FLY_TARGETS,  // the towns FLY can go to now
    PD_REQ_FLY,          // to mapsec
};

#define PD_FLY_MAX 64
struct pd_act {
    // The request (the companion's), run on the host's next step.
    enum pd_act_kind kind;
    int item, slot, move, mapsec;
    uint32_t personality;
    // What it gave back: the result (a message), the party pick, FLY's towns.
    enum pd_act_kind done; // the request this answers (PD_REQ_NONE = nothing new)
    enum pd_act_result result;
    int steps, swapped; // a Repel's steps; GIVE's / TAKE's other item
    int targetCount;
    struct pd_target targets[PD_PARTY_SIZE];
    int flyCount, flyerSlot;
    uint32_t flyerPersonality;
    uint16_t fly[PD_FLY_MAX];
    // FLY's second step, frames away (the fade to black).
    int flyWait, flyMapsec;
    int tries; // frames a request has waited for a safe moment
};

// What the ROM allows (fixed per ROM, for drawing the bag): an item's USE / GIVE,
// whether USE picks a Pokémon / a move, TAKE, PC, FLY.
bool pd_act_supported(const struct pd_game* g);
bool pd_act_can_use(const struct pd_game* g, int item);
bool pd_act_can_give(const struct pd_game* g, int item);
bool pd_act_picks_mon(const struct pd_game* g, int item);
bool pd_act_picks_move(const struct pd_game* g, int item);
bool pd_act_pp_boost(int item); // PP Up / PP Max: "Boost PP of which move?"
bool pd_act_can_take(const struct pd_game* g);
bool pd_act_can_pc(const struct pd_game* g);
bool pd_act_can_fly(const struct pd_game* g);

// Asks for kind (the fields it needs set first); replaces one not run yet.
void pd_act_request(struct pd_act* a, enum pd_act_kind kind);
// The host, after each frame: runs a pending request when the game can be
// called, and FLY's second step. inBattle from the snapshot. True when there's
// something new for the companion (a.done).
bool pd_act_step(struct pd_act* a, const struct pd_game* g, const struct pd_game_io* io, bool inBattle);
// The message a result shows ("Used it.", "Not now." ...), "" for none.
// s names the Pokémon (the request's slot).
const char* pd_act_message(const struct pd_act* a, const struct pd_game* g, const struct pd_snapshot* s, char* buf,
                           size_t len);

#endif
