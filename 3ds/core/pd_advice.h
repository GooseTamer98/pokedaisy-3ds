// Whether a Pokémon should learn a move, and in place of which - the app's
// MoveAdvisor (MoveAdvice.kt) and MoveLearning, over the original games' move
// data (pd_move_stats). The summary's LEARN page.
#ifndef PD_ADVICE_H
#define PD_ADVICE_H

#include <stdbool.h>

#include "pd_game.h"
#include "pd_snapshot.h"

enum pd_verdict {
    PD_LEARN,   // a free slot: the game teaches it without asking
    PD_REPLACE, // worth it, in place of forget
    PD_SKIP,    // the moves it has are better
    PD_KNOWN,   // it knows it already
};

#define PD_REASONS 3
struct pd_reason {
    char text[72];
    bool good; // for the Pokémon or not
};

struct pd_advice {
    int move;
    enum pd_verdict verdict;
    int forget; // the move to forget, 0 = none
    float gain;
    int reasonCount;
    struct pd_reason reasons[PD_REASONS];
};

// A move m could learn: at level (a level-up), or from the bag's TM ("TM35").
struct pd_learn_option {
    int move, level;
    char tm[8];
    struct pd_advice advice;
};

#define PD_LEARN_MAX 24
// m's next few level-up moves it doesn't know, then the bag's TMs / HMs it can
// learn, each with its advice - the bag's best first (MoveLearning.options).
int pd_learn_options(const struct pd_game* g, const struct pd_snapshot* s, const struct pd_mon* m,
                     struct pd_learn_option* out, int max);

// The bag's TM / HM item's move (TM01 = item 289 .. HM08 = 346, sTMHMMoves),
// 0 = not one, or the table isn't known.
int pd_tm_item_move(const struct pd_game* g, int item);

// Who in the party can learn a TM / HM (MoveLearning.learners): one per party
// slot, the advice for each who can. Returns the party's size, 0 = not a TM.
enum pd_tm_fit { PD_TM_NOT_ABLE, PD_TM_ABLE, PD_TM_LEARNED };
struct pd_tm_learner {
    enum pd_tm_fit fit;
    struct pd_advice advice; // ABLE only
};
int pd_tm_learners(const struct pd_game* g, const struct pd_snapshot* s, int item, struct pd_tm_learner* out);
// The party slot it suits best: the biggest gain short of a SKIP, -1 = none.
int pd_tm_best(const struct pd_tm_learner* l, int n);

#endif
