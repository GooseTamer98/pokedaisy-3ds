#include "pd_advice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pd_dex.h"
#include "pd_tables.h"

// MoveAdvisor's moveset score:
//  - attacks: for every defending type, the best hit it has against it (power x
//    accuracy, x1.5 for its own type, scaled by the stat the move runs on - Gen
//    3 splits physical / special by type), averaged over the types;
//  - status moves: a tier by effect, worth a share of that attack score, less
//    for a second of a kind.
// The best swap raises the score most; under MIN_GAIN it's a SKIP (MIN_GAIN_HM
// to learn an HM, which can't be forgotten again).
#define MIN_GAIN 0.1f
#define MIN_GAIN_HM 0.3f
#define TIE 0.01f
#define TIER_SHARE 0.08f
#define BASELINE 30.0f
#define TYPE_MYSTERY 9
#define UPCOMING 4
#define ITEM_TM01 289
#define ITEM_HM08 346

// The original games' HMs (CUT, FLY, SURF, STRENGTH, FLASH, ROCK SMASH,
// WATERFALL, DIVE): the game won't let a Pokémon forget one.
static const int HM_MOVES[8] = { 15, 19, 57, 70, 148, 249, 127, 291 };

struct mon {
    int t1, t2;
    int attack, spAttack; // 0 = unknown
    int level;
    int moves[4], count;
};

static bool is_hm(int move) {
    for (int i = 0; i < 8; i++) {
        if (HM_MOVES[i] == move) return true;
    }
    return false;
}

static const struct pd_move_stats* stats_of(int move) {
    return move > 0 && move < pd_move_stats_count ? &pd_move_stats[move] : NULL;
}

static bool is_status(int move) {
    return pd_move_power(move) == 0;
}

static bool has_type(const struct mon* m, int type) {
    return type == m->t1 || type == m->t2;
}

static bool physical(int type) {
    return type < TYPE_MYSTERY; // Normal ... Steel physical, Fire ... Dark special
}

// The stat type's moves run on, against the Pokémon's better one (1 = its best).
static float stat_share(const struct mon* m, int type) {
    if (!m->attack || !m->spAttack) return 1;
    int best = m->attack > m->spAttack ? m->attack : m->spAttack;
    float f = (float) (physical(type) ? m->attack : m->spAttack) / (float) best;
    return f < 0.4f ? 0.4f : f;
}

static bool is(const char* effect, const char* const* names) {
    for (; *names; names++) {
        if (!strcmp(effect, *names)) return true;
    }
    return false;
}
#define IS(effect, ...) is(effect, (const char* const[]) { __VA_ARGS__, NULL })

// A hit's worth to this Pokémon before the type chart; < 0 = a status move.
static float attack_value(const struct mon* m, int move) {
    const struct pd_move_stats* st = stats_of(move);
    int power = pd_move_power(move);
    if (!st || power == 0) return -1;
    float p = (float) power;
    bool ownType = true; // fixed-damage moves ignore both the type bonus and the stats
    const char* e = st->effect;
    if (!strcmp(e, "MULTI_HIT")) p *= 3;
    else if (IS(e, "DOUBLE_HIT", "TWINEEDLE")) p *= 2;
    else if (!strcmp(e, "TRIPLE_KICK")) p = 50;
    else if (!strcmp(e, "OHKO")) { p = 90; ownType = false; }
    else if (!strcmp(e, "LEVEL_DAMAGE")) { p = (float) (m->level < 10 ? 10 : m->level > 100 ? 100 : m->level); ownType = false; }
    else if (IS(e, "SONICBOOM", "DRAGON_RAGE")) { p = 35; ownType = false; }
    else if (IS(e, "SUPER_FANG", "PSYWAVE", "ENDEAVOR")) { p = 40; ownType = false; }
    else if (IS(e, "COUNTER", "MIRROR_COAT", "BIDE")) { p = 40; ownType = false; }
    else if (IS(e, "RETURN", "FRUSTRATION")) p = 70;
    else if (IS(e, "FLAIL", "LOW_KICK", "MAGNITUDE", "WEATHER_BALL", "SPIT_UP", "BEAT_UP", "PRESENT")) p = 55;
    else if (!strcmp(e, "HIDDEN_POWER")) p = 50;
    else if (!strcmp(e, "ERUPTION")) p *= 0.7f;
    else if (IS(e, "RECHARGE", "RAZOR_WIND", "SKULL_BASH", "SKY_ATTACK", "SOLAR_BEAM", "FOCUS_PUNCH")) p *= 0.6f;
    else if (IS(e, "SEMI_INVULNERABLE", "RAMPAGE")) p *= 0.85f;
    else if (IS(e, "RECOIL", "DOUBLE_EDGE", "RECOIL_IF_MISS")) p *= 0.9f;
    else if (IS(e, "OVERHEAT", "SUPERPOWER")) p *= 0.8f;
    else if (!strcmp(e, "EXPLOSION")) p *= 0.25f;
    else if (IS(e, "FAKE_OUT", "SNORE", "DREAM_EATER")) p *= 0.4f;
    else if (IS(e, "FALSE_SWIPE", "FUTURE_SIGHT")) p *= 0.75f;
    else if (IS(e, "ROLLOUT", "FURY_CUTTER")) p *= 1.5f;
    float acc = st->accuracy == 0 ? 1 : st->accuracy / 100.0f;
    if (!ownType) return p * acc;
    int type = pd_move_type(move);
    float stab = has_type(m, type) ? 1.5f : 1;
    return p * acc * stab * stat_share(m, type) * (st->priority > 0 ? 1.1f : 1);
}

// The mean, over the defending types, of the best hit the moveset has on each.
static float attack_score(const struct mon* m, const int* moves, int n) {
    float sum = 0;
    int types = 0;
    bool any = false;
    for (int def = 0; def < pd_type_names_count; def++) {
        if (def == TYPE_MYSTERY || !pd_type_names[def]) continue;
        types++;
        float best = 0;
        for (int i = 0; i < n; i++) {
            float v = attack_value(m, moves[i]);
            if (v < 0) continue;
            any = true;
            float hit = v * pd_type_multiplier_pct(pd_move_type(moves[i]), def, PD_TYPE_NONE) / 100.0f;
            if (hit > best) best = hit;
        }
        sum += best;
    }
    return any && types ? sum / types : 0;
}

// ---- status moves ----

enum group { INFLICT, SETUP_ATK, SETUP_SPA, SETUP_OTHER, HEAL, SHIELD, DEBUFF, WEATHER, OTHER, GROUPS };

static float status_tier(const char* e, enum group* g) {
    if (IS(e, "SLEEP", "PARALYZE", "TOXIC")) { *g = INFLICT; return 3; }
    if (IS(e, "WILL_O_WISP", "POISON", "YAWN", "LEECH_SEED")) { *g = INFLICT; return 2; }
    if (IS(e, "CONFUSE", "ATTRACT", "SWAGGER", "FLATTER", "TEETER_DANCE")) { *g = INFLICT; return 1; }
    if (IS(e, "ATTACK_UP_2", "DRAGON_DANCE", "BULK_UP", "BELLY_DRUM", "CURSE")) { *g = SETUP_ATK; return 3; }
    if (!strcmp(e, "ATTACK_UP")) { *g = SETUP_ATK; return 1.5f; }
    if (IS(e, "CALM_MIND", "SPECIAL_ATTACK_UP_2")) { *g = SETUP_SPA; return 3; }
    if (!strcmp(e, "SPECIAL_ATTACK_UP")) { *g = SETUP_SPA; return 1.5f; }
    if (IS(e, "SPEED_UP_2", "COSMIC_POWER", "DEFENSE_UP_2", "SPECIAL_DEFENSE_UP_2", "EVASION_UP", "MINIMIZE")) {
        *g = SETUP_OTHER;
        return 2;
    }
    if (IS(e, "DEFENSE_UP", "DEFENSE_CURL", "FOCUS_ENERGY", "CHARGE", "STOCKPILE")) { *g = SETUP_OTHER; return 1; }
    if (IS(e, "RESTORE_HP", "SOFTBOILED", "MOONLIGHT", "MORNING_SUN", "SYNTHESIS", "WISH")) { *g = HEAL; return 3; }
    if (!strcmp(e, "PAIN_SPLIT")) { *g = HEAL; return 2; }
    if (!strcmp(e, "REST")) { *g = HEAL; return 1.5f; }
    if (IS(e, "INGRAIN", "SWALLOW")) { *g = HEAL; return 1; }
    if (IS(e, "SUBSTITUTE", "REFLECT", "LIGHT_SCREEN")) { *g = SHIELD; return 2; }
    if (!strcmp(e, "PROTECT")) { *g = SHIELD; return 1.5f; }
    if (IS(e, "ENDURE", "SAFEGUARD", "MIST")) { *g = SHIELD; return 0.7f; }
    if (IS(e, "ATTACK_DOWN_2", "DEFENSE_DOWN_2", "SPEED_DOWN_2", "SPECIAL_DEFENSE_DOWN_2")) { *g = DEBUFF; return 1.5f; }
    if (!strcmp(e, "ACCURACY_DOWN")) { *g = DEBUFF; return 1; }
    if (IS(e, "ATTACK_DOWN", "DEFENSE_DOWN", "SPEED_DOWN", "EVASION_DOWN", "TICKLE")) { *g = DEBUFF; return 0.5f; }
    if (IS(e, "RAIN_DANCE", "SUNNY_DAY", "SANDSTORM", "HAIL")) { *g = WEATHER; return 1; }
    *g = OTHER;
    if (IS(e, "HEAL_BELL", "BATON_PASS", "TAUNT", "ENCORE", "DISABLE", "SPIKES", "HAZE", "PERISH_SONG", "DESTINY_BOND")) {
        return 1;
    }
    if (IS(e, "SPLASH", "TELEPORT", "WATER_SPORT", "MUD_SPORT", "CAMOUFLAGE", "CONVERSION", "CONVERSION_2", "SKETCH",
           "FOLLOW_ME", "HELPING_HAND", "GRUDGE", "SNATCH", "IMPRISON", "RECYCLE", "ROLE_PLAY", "NIGHTMARE")) {
        return 0;
    }
    return 0.5f;
}

// A status move's tier for this Pokémon: a boost to the stat it doesn't attack with is worth little.
static float tier_for(const struct mon* m, const char* e, enum group* g) {
    float t = status_tier(e, g);
    if (!m->attack || !m->spAttack) return t;
    if (*g == SETUP_ATK && m->attack < m->spAttack * 0.8f) return 0.5f;
    if (*g == SETUP_SPA && m->spAttack < m->attack * 0.8f) return 0.5f;
    return t;
}

// What a hit does besides damage, at the full effect's tier (its chance scales it); < 0 = nothing.
static float side_effect(const char* e, enum group* g) {
    if (IS(e, "PARALYZE_HIT", "THUNDER", "FREEZE_HIT", "POISON_FANG")) { *g = INFLICT; return 3; }
    if (IS(e, "BURN_HIT", "BLAZE_KICK", "POISON_HIT", "POISON_TAIL", "TRI_ATTACK")) { *g = INFLICT; return 2; }
    if (!strcmp(e, "CONFUSE_HIT")) { *g = INFLICT; return 1; }
    if (IS(e, "FLINCH_HIT", "FLINCH_MINIMIZE_HIT")) { *g = OTHER; return 1; }
    if (IS(e, "ATTACK_DOWN_HIT", "DEFENSE_DOWN_HIT", "SPEED_DOWN_HIT", "SPECIAL_ATTACK_DOWN_HIT",
           "SPECIAL_DEFENSE_DOWN_HIT", "ACCURACY_DOWN_HIT")) {
        *g = DEBUFF;
        return 1;
    }
    if (IS(e, "ATTACK_UP_HIT", "DEFENSE_UP_HIT", "ALL_STATS_UP_HIT")) { *g = SETUP_OTHER; return 1; }
    return -1;
}

static float effect_tier(const struct mon* m, int move, enum group* g) {
    const struct pd_move_stats* st = stats_of(move);
    if (!st) return -1;
    if (is_status(move)) return tier_for(m, st->effect, g);
    float t = side_effect(st->effect, g);
    if (t < 0) return -1;
    int ch = st->chance < 0 ? 0 : st->chance > 100 ? 100 : st->chance;
    return t * ch / 100.0f;
}

static float status_score(const struct mon* m, const int* moves, int n, float ref) {
    float tiers[4];
    enum group groups[4];
    int k = 0;
    for (int i = 0; i < n; i++) {
        enum group g;
        float t = effect_tier(m, moves[i], &g);
        if (t < 0) continue;
        tiers[k] = t;
        groups[k] = g;
        k++;
    }
    // Highest first, a second of a kind counting for little.
    int order[4] = { 0, 1, 2, 3 };
    for (int i = 0; i < k; i++) {
        for (int j = i + 1; j < k; j++) {
            if (tiers[order[j]] > tiers[order[i]]) {
                int t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
    }
    int seen[GROUPS] = { 0 };
    float sum = 0;
    for (int i = 0; i < k; i++) {
        int o = order[i];
        sum += tiers[o] * (++seen[groups[o]] == 1 ? 1 : 0.3f);
    }
    return sum * TIER_SHARE * ref;
}

static float score(const struct mon* m, const int* moves, int n, float ref) {
    return attack_score(m, moves, n) + status_score(m, moves, n, ref);
}

// ---- reasons ----

// The types the moveset hits super effectively, a bit per type id.
static unsigned super_types(const struct mon* m, const int* moves, int n) {
    unsigned bits = 0;
    for (int def = 0; def < pd_type_names_count; def++) {
        if (def == TYPE_MYSTERY || !pd_type_names[def]) continue;
        for (int i = 0; i < n; i++) {
            if (attack_value(m, moves[i]) < 0) continue;
            if (pd_type_multiplier_pct(pd_move_type(moves[i]), def, PD_TYPE_NONE) > 100) bits |= 1u << def;
        }
    }
    return bits;
}

static void type_list(unsigned bits, char* buf, size_t len) {
    buf[0] = 0;
    size_t used = 0;
    int shown = 0;
    for (int t = 0; t < 32 && shown < 3; t++) {
        if (!(bits >> t & 1)) continue;
        used += (size_t) snprintf(buf + used, len - used, "%s%s", shown ? ", " : "", pd_type_name(t));
        if (used >= len) break;
        shown++;
    }
}

static void add_reason(struct pd_advice* a, bool good, const char* fmt, const char* arg1, const char* arg2) {
    if (a->reasonCount >= PD_REASONS) return;
    struct pd_reason* r = &a->reasons[a->reasonCount++];
    snprintf(r->text, sizeof(r->text), fmt, arg1 ? arg1 : "", arg2 ? arg2 : "");
    r->good = good;
}

static void add_bad(struct pd_reason* bad, int* count, const char* fmt, const char* x1, const char* x2) {
    if (*count >= 4) return;
    snprintf(bad[*count].text, sizeof(bad[0].text), fmt, x1 ? x1 : "", x2 ? x2 : "");
    bad[(*count)++].good = false;
}

// Why learning move (in place of out, if any) is worth it: gathered, then
// the good ones first, three in all.
static void why_new(struct pd_advice* a, const struct mon* m, int move, int out, const int* cur, int n, float ref) {
    char buf[40], b2[12], b3[12], name[32];
    int after[4];
    int an = 0;
    for (int i = 0; i < n; i++) after[an++] = cur[i] == out ? move : cur[i];
    if (!out) after[an++] = move;
    struct pd_advice all = { 0 };
    struct pd_advice* t = &all;
    int type = pd_move_type(move);
    const struct pd_move_stats* st = stats_of(move);
    // t holds up to PD_REASONS: the bad ones are added last, so the cap keeps the good.
    struct pd_reason bad[4];
    int badCount = 0;
#define GOOD(...) add_reason(t, true, __VA_ARGS__)
#define BAD(...) add_bad(bad, &badCount, __VA_ARGS__)
    if (!is_status(move)) {
        unsigned gained = super_types(m, after, an) & ~super_types(m, cur, n);
        if (gained) {
            type_list(gained, buf, sizeof(buf));
            GOOD("NEW 2x ON %s%s", buf, NULL);
        }
        if (has_type(m, type)) GOOD("YOUR TYPE: x1.5%s%s", NULL, NULL);
        if (out && !is_status(out) && pd_move_type(out) == type && pd_move_power(move) > pd_move_power(out)) {
            snprintf(b2, sizeof(b2), "%d", pd_move_power(move));
            snprintf(b3, sizeof(b3), "%d", pd_move_power(out));
            GOOD("SAME TYPE, %s vs %s POWER", b2, b3);
        }
        if (stat_share(m, type) < 0.8f) {
            BAD(physical(type) ? "RUNS ON ATTACK, ITS WEAKER STAT%s%s" : "RUNS ON SP. ATK, ITS WEAKER STAT%s%s",
                NULL, NULL);
        }
        if (st && st->accuracy >= 1 && st->accuracy <= 84) {
            snprintf(b2, sizeof(b2), "%d", st->accuracy);
            BAD("ACCURACY %s%%%s", b2, NULL);
        }
    } else if (st) {
        enum group g;
        if (tier_for(m, st->effect, &g) >= 2) GOOD("STRONG EFFECT%s%s", NULL, NULL);
    }
    if (out) {
        unsigned lost = super_types(m, cur, n) & ~super_types(m, after, an);
        if (lost) {
            type_list(lost, buf, sizeof(buf));
            BAD("LOSES 2x ON %s%s", buf, NULL);
        }
        // What the forgotten move adds to the rest: a weak effect, or hits the others land harder.
        int rest[4], rn = 0;
        for (int i = 0; i < n; i++) {
            if (cur[i] != out) rest[rn++] = cur[i];
        }
        float adds = (score(m, cur, n, ref) - score(m, rest, rn, ref)) / ref;
        if (adds < MIN_GAIN) GOOD("%s DOES LITTLE%s", pd_move_name(out, name, sizeof(name)), NULL);
    }
#undef GOOD
#undef BAD
    for (int i = 0; i < badCount; i++) {
        if (t->reasonCount < PD_REASONS) t->reasons[t->reasonCount++] = bad[i];
    }
    for (int i = 0; i < t->reasonCount; i++) {
        if (a->reasonCount < PD_REASONS) a->reasons[a->reasonCount++] = t->reasons[i];
    }
}

static void why_skip(struct pd_advice* a, const struct mon* m, int move, const int* cur, int n) {
    char b2[12], name[32];
    int type = pd_move_type(move);
    const struct pd_move_stats* st = stats_of(move);
    int start = a->reasonCount;
    if (!is_status(move)) {
        float mine = attack_value(m, move);
        int better = 0;
        float betterV = -1;
        for (int i = 0; i < n; i++) {
            if (is_status(cur[i]) || pd_move_type(cur[i]) != type) continue;
            float v = attack_value(m, cur[i]);
            if (v >= mine && v > betterV) {
                betterV = v;
                better = cur[i];
            }
        }
        if (better) add_reason(a, false, "%s IS BETTER%s", pd_move_name(better, name, sizeof(name)), NULL);
        if (stat_share(m, type) < 0.8f) {
            add_reason(a, false, physical(type) ? "RUNS ON ATTACK, ITS WEAKER STAT%s%s" : "RUNS ON SP. ATK, ITS WEAKER STAT%s%s",
                       NULL, NULL);
        }
        if (st && st->accuracy >= 1 && st->accuracy <= 84) {
            snprintf(b2, sizeof(b2), "%d", st->accuracy);
            add_reason(a, false, "ACCURACY %s%%%s", b2, NULL);
        }
    } else if (st) {
        enum group g;
        if (tier_for(m, st->effect, &g) <= 1) add_reason(a, false, "WEAK EFFECT%s%s", NULL, NULL);
    }
    if (a->reasonCount == start) add_reason(a, false, "ITS MOVES ARE BETTER%s%s", NULL, NULL);
    if (a->reasonCount > start + 2) a->reasonCount = start + 2;
}

static void advise(const struct mon* m, int move, struct pd_advice* a) {
    memset(a, 0, sizeof(*a));
    a->move = move;
    for (int i = 0; i < m->count; i++) {
        if (m->moves[i] == move) {
            a->verdict = PD_KNOWN;
            return;
        }
    }
    const int* cur = m->moves;
    int n = m->count;
    float ref = attack_score(m, cur, n);
    if (ref <= 0) ref = BASELINE;
    float now = score(m, cur, n, ref);
    if (n < 4) {
        int with[4];
        memcpy(with, cur, sizeof(int) * (size_t) n);
        with[n] = move;
        a->verdict = PD_LEARN;
        a->gain = (score(m, with, n + 1, ref) - now) / ref;
        add_reason(a, true, "FREE SLOT%s%s", NULL, NULL);
        why_new(a, m, move, 0, cur, n, ref);
        return;
    }
    // Every forgettable move swapped out; the best (ties: a weaker move of the new move's type).
    float scores[4];
    int newType = pd_move_type(move);
    float top = -1;
    for (int i = 0; i < n; i++) {
        scores[i] = -1;
        if (is_hm(cur[i])) continue;
        int swapped[4];
        for (int k = 0; k < n; k++) swapped[k] = k == i ? move : cur[k];
        scores[i] = score(m, swapped, n, ref);
        if (scores[i] > top) top = scores[i];
    }
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (scores[i] < 0 || (top - scores[i]) / ref > TIE) continue;
        if (best < 0) {
            best = i;
            continue;
        }
        bool sameI = !is_status(cur[i]) && pd_move_type(cur[i]) == newType;
        bool sameB = !is_status(cur[best]) && pd_move_type(cur[best]) == newType;
        if (sameI != sameB) {
            if (sameI) best = i;
            continue;
        }
        float vi = attack_value(m, cur[i]), vb = attack_value(m, cur[best]);
        if (vi < 0) vi = 1e9f;
        if (vb < 0) vb = 1e9f;
        if (vi < vb) best = i;
    }
    a->gain = best >= 0 ? (scores[best] - now) / ref : 0;
    bool hm = is_hm(move);
    if (best < 0 || a->gain < (hm ? MIN_GAIN_HM : MIN_GAIN)) {
        a->verdict = PD_SKIP;
        if (hm && a->gain >= MIN_GAIN) add_reason(a, false, "AN HM: IT STAYS FOR GOOD%s%s", NULL, NULL);
        else why_skip(a, m, move, cur, n);
        return;
    }
    a->verdict = PD_REPLACE;
    a->forget = cur[best];
    why_new(a, m, move, cur[best], cur, n, ref);
}

static void mon_of(const struct pd_mon* p, struct mon* m) {
    memset(m, 0, sizeof(*m));
    pd_species_type_pair(p->species, &m->t1, &m->t2);
    m->attack = p->stats[1];
    m->spAttack = p->stats[4];
    m->level = p->level;
    for (int i = 0; i < 4; i++) {
        if (p->moves[i]) m->moves[m->count++] = p->moves[i];
    }
}

static int by_bag_best(const void* x, const void* y) {
    const struct pd_learn_option *a = x, *b = y;
    bool sa = a->advice.verdict == PD_SKIP, sb = b->advice.verdict == PD_SKIP;
    if (sa != sb) return sa - sb;
    return (b->advice.gain > a->advice.gain) - (b->advice.gain < a->advice.gain);
}

int pd_learn_options(const struct pd_game* g, const struct pd_snapshot* s, const struct pd_mon* p,
                     struct pd_learn_option* out, int max) {
    if (p->species <= 0 || p->species == PD_SPECIES_EGG) return 0;
    struct mon m;
    mon_of(p, &m);
    int n = 0;
    // The next few level-up moves it doesn't know.
    struct pd_level_move lv[64];
    int ln = pd_level_moves(g, p->species, lv, 64);
    for (int i = 0; i < ln && n < max && n < UPCOMING; i++) {
        if (lv[i].level <= p->level) continue;
        bool skip = false;
        for (int k = 0; k < m.count && !skip; k++) skip = m.moves[k] == lv[i].move;
        for (int k = 0; k < n && !skip; k++) skip = out[k].move == lv[i].move;
        if (skip) continue;
        struct pd_learn_option* o = &out[n++];
        memset(o, 0, sizeof(*o));
        o->move = lv[i].move;
        o->level = lv[i].level;
        advise(&m, o->move, &o->advice);
    }
    // The bag's TMs / HMs it can learn, the best first.
    struct pd_tm_move tms[64];
    int tn = pd_tm_moves(g, p->species, tms, 64);
    int first = n;
    const struct pd_pocket_items* pocket = &s->bag[PD_POCKET_TMHM];
    for (int i = 0; i < pocket->count && n < max; i++) {
        int item = pocket->items[i].id;
        if (item < ITEM_TM01 || item > ITEM_HM08) continue;
        char label[8];
        int idx = item - ITEM_TM01;
        snprintf(label, sizeof(label), idx < 50 ? "TM%02d" : "HM%02d", idx < 50 ? idx + 1 : idx - 49);
        int move = 0;
        for (int k = 0; k < tn; k++) {
            if (!strcmp(tms[k].label, label)) move = tms[k].move;
        }
        bool known = false;
        for (int k = 0; k < m.count && !known; k++) known = m.moves[k] == move;
        if (!move || known) continue;
        struct pd_learn_option* o = &out[n++];
        memset(o, 0, sizeof(*o));
        o->move = move;
        snprintf(o->tm, sizeof(o->tm), "%s", label);
        advise(&m, move, &o->advice);
    }
    qsort(out + first, (size_t) (n - first), sizeof(out[0]), by_bag_best);
    return n;
}

int pd_tm_item_move(const struct pd_game* g, int item) {
    if (item < ITEM_TM01 || item > ITEM_HM08 || !g->cfg || !g->cfg->tmhmMoves) return 0;
    const uint8_t* e = pd_rom_at(g, g->cfg->tmhmMoves + (uint32_t) (item - ITEM_TM01) * 2, 2);
    return e ? e[0] | e[1] << 8 : 0;
}

int pd_tm_learners(const struct pd_game* g, const struct pd_snapshot* s, int item, struct pd_tm_learner* out) {
    int move = pd_tm_item_move(g, item);
    if (!move) return 0;
    char label[8];
    int idx = item - ITEM_TM01;
    snprintf(label, sizeof(label), idx < 50 ? "TM%02d" : "HM%02d", idx < 50 ? idx + 1 : idx - 49);
    int n = 0;
    for (int i = 0; i < s->partyCount && i < PD_PARTY_SIZE; i++) {
        const struct pd_mon* p = &s->party[i];
        struct pd_tm_learner* l = &out[n++];
        memset(l, 0, sizeof(*l));
        l->fit = PD_TM_NOT_ABLE;
        if (p->species <= 0 || p->species == PD_SPECIES_EGG) continue;
        struct mon m;
        mon_of(p, &m);
        bool known = false;
        for (int k = 0; k < m.count && !known; k++) known = m.moves[k] == move;
        if (known) {
            l->fit = PD_TM_LEARNED;
            continue;
        }
        struct pd_tm_move tms[64];
        int tn = pd_tm_moves(g, p->species, tms, 64);
        for (int k = 0; k < tn; k++) {
            if (!strcmp(tms[k].label, label)) l->fit = PD_TM_ABLE;
        }
        if (l->fit == PD_TM_ABLE) advise(&m, move, &l->advice);
    }
    return n;
}

int pd_tm_best(const struct pd_tm_learner* l, int n) {
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (l[i].fit != PD_TM_ABLE || l[i].advice.verdict == PD_SKIP) continue;
        if (best < 0 || l[i].advice.gain > l[best].advice.gain) best = i;
    }
    return best;
}
