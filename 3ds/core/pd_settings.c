#include "pd_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void pd_settings_defaults(struct pd_settings* s) {
    s->screenMode = PD_SCREEN_SHARP;
    s->ffSpeed = PD_FF_MIN;
    s->guideNotice = 0;
    memset(s->favorites, 0, sizeof(s->favorites));
}

static void game_key(const struct pd_game* g, char* buf, size_t len) {
    snprintf(buf, len, "%.4s%d", g->code, g->revision & 0xF);
}

struct pd_favorites* pd_favorites_for(struct pd_settings* s, const struct pd_game* g, bool create) {
    char key[8];
    game_key(g, key, sizeof(key));
    for (int i = 0; i < PD_FAV_GAMES; i++) {
        if (!strcmp(s->favorites[i].game, key)) return &s->favorites[i];
    }
    if (!create) return NULL;
    struct pd_favorites* f = NULL;
    for (int i = 0; i < PD_FAV_GAMES && !f; i++) {
        if (!s->favorites[i].game[0]) f = &s->favorites[i];
    }
    if (!f) {
        // Full: the first slot goes, the rest move up.
        memmove(&s->favorites[0], &s->favorites[1], sizeof(s->favorites[0]) * (PD_FAV_GAMES - 1));
        f = &s->favorites[PD_FAV_GAMES - 1];
    }
    memset(f, 0, sizeof(*f));
    snprintf(f->game, sizeof(f->game), "%s", key);
    return f;
}

bool pd_favorite(const struct pd_favorites* f, int item) {
    for (int i = 0; f && i < f->count; i++) {
        if (f->items[i] == item) return true;
    }
    return false;
}

void pd_favorite_toggle(struct pd_settings* s, const struct pd_game* g, int item) {
    struct pd_favorites* f = pd_favorites_for(s, g, true);
    for (int i = 0; i < f->count; i++) {
        if (f->items[i] == item) {
            memmove(&f->items[i], &f->items[i + 1], sizeof(f->items[0]) * (size_t) (f->count - i - 1));
            f->count--;
            return;
        }
    }
    if (f->count < PD_FAV_MAX) f->items[f->count++] = (uint16_t) item;
}

// "fav.BPRE1=13,4,22"
static void load_favorites(struct pd_settings* s, const char* game, const char* list) {
    struct pd_favorites* f = NULL;
    for (int i = 0; i < PD_FAV_GAMES && !f; i++) {
        if (!s->favorites[i].game[0]) f = &s->favorites[i];
    }
    if (!f || strlen(game) >= sizeof(f->game) || !*game) return;
    snprintf(f->game, sizeof(f->game), "%s", game);
    const char* p = list;
    while (*p && f->count < PD_FAV_MAX) {
        char* end;
        long v = strtol(p, &end, 10);
        if (end == p) break;
        if (v > 0 && v < 0x10000) f->items[f->count++] = (uint16_t) v;
        p = *end == ',' ? end + 1 : end;
        if (*end != ',') break;
    }
}

static bool int_in(const char* v, int lo, int hi, int* out) {
    char* end;
    long n = strtol(v, &end, 10);
    if (end == v || n < lo || n > hi) return false;
    *out = (int) n;
    return true;
}

bool pd_settings_load(struct pd_settings* s, const char* path) {
    pd_settings_defaults(s);
    FILE* f = fopen(path, "r");
    if (!f) return false;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char* key = line;
        const char* value = eq + 1;
        if (!strcmp(key, "screen")) int_in(value, 0, PD_SCREEN_MODES - 1, &s->screenMode);
        else if (!strcmp(key, "ff")) int_in(value, PD_FF_MIN, PD_FF_MAX, &s->ffSpeed);
        else if (!strcmp(key, "guide")) int_in(value, 0, 0xFFFF, &s->guideNotice);
        else if (!strncmp(key, "fav.", 4)) load_favorites(s, key + 4, value);
    }
    fclose(f);
    return true;
}

bool pd_settings_save(const struct pd_settings* s, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "screen=%d\nff=%d\nguide=%d\n", s->screenMode, s->ffSpeed, s->guideNotice);
    for (int i = 0; i < PD_FAV_GAMES; i++) {
        const struct pd_favorites* v = &s->favorites[i];
        if (!v->game[0] || !v->count) continue;
        fprintf(f, "fav.%s=", v->game);
        for (int k = 0; k < v->count; k++) fprintf(f, k ? ",%d" : "%d", v->items[k]);
        fputc('\n', f);
    }
    return fclose(f) == 0;
}
