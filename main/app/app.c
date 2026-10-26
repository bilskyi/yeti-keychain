#include "app/app_internal.h"

#include <stddef.h>
#include <string.h>

#define SLIDE_MS 180
#define LEVELUP_MS 1800
#define PET_MINUTE_MS 60000u
#define PET_SAVE_EVERY 10  // pet minutes between saves (flash wear)

static const char *const ITEM_NAMES[ITEM_COUNT] = {
    "FEED", "GAMES", "PET", "SLEEP", "SOUND", "SETUP", "BACK",
    "DICE", "COIN", "REACTION", "CATCH", "FLAPPY", "SNAKE", "MEMORY", "MUSIC", "MENU",
};

const char *app_item_name(app_item_t it) {
    return (unsigned)it < ITEM_COUNT ? ITEM_NAMES[it] : "?";
}

static const app_item_t TOP[] = {ITEM_FEED, ITEM_GAMES, ITEM_STATS, ITEM_SLEEP,
                                 ITEM_SOUND, ITEM_SETUP, ITEM_BACK};
static const app_item_t GAMES[] = {ITEM_DICE,   ITEM_COIN,  ITEM_REACTION, ITEM_CATCH,
                                   ITEM_FLAPPY, ITEM_SNAKE, ITEM_MEMORY,   ITEM_MUSIC, ITEM_UP};
static const struct {
    const app_item_t *items;
    int n;
} LISTS[2] = {
    {TOP, (int)(sizeof(TOP) / sizeof(TOP[0]))},
    {GAMES, (int)(sizeof(GAMES) / sizeof(GAMES[0]))},
};

// ---------------------------------------------------------------------------
// Helpers shared with the games
// ---------------------------------------------------------------------------

void app_play(const app_t *a, app_sound_t s) {
    if (a->env && a->env->play) a->env->play(s);
}

static void save(const app_t *a) {
    if (a->env && a->env->save) a->env->save(&a->save);
}

bool app_record(app_t *a, uint16_t *best, uint16_t value, bool lower_is_better) {
    const bool better = *best == 0 || (lower_is_better ? value < *best : value > *best);
    if (!better || value == 0) return false;
    *best = value;
    save(a);
    return true;
}

void app_reward(app_t *a, int fun, int energy, int xp) {
    pet_play(&a->save.pet, fun, energy);
    if (pet_add_xp(&a->save.pet, xp)) {
        a->levelup = true;
        a->levelup_t0 = a->now;
        a->mood = EXPR_EXCITED;
        app_play(a, SND_LEVELUP);
        save(a);
    }
}

void app_game_over(gfx_t *g, int score, bool best, int saved_best) {
    char line[20];
    font_draw_centered(g, 64, 6, "GAME OVER", 2, GFX_WHITE);
    app_itoa(score, line);
    font_draw_centered(g, 64, 24, line, 2, GFX_WHITE);
    if (best) {
        font_draw_centered(g, 64, 42, "NEW BEST!", 1, GFX_WHITE);
    } else {
        char b[20] = "BEST ";
        app_itoa(saved_best, b + 5);
        font_draw_centered(g, 64, 42, b, 1, GFX_WHITE);
    }
    app_hint(g, "A:AGAIN", "B:MENU");
}

void app_hint(gfx_t *g, const char *a_does, const char *b_does) {
    font_draw(g, 2, 57, a_does, 1, GFX_WHITE);
    font_draw(g, GFX_W - 2 - font_width(b_does, 1), 57, b_does, 1, GFX_WHITE);
}

void app_bar(gfx_t *g, int x, int y, int w, int h, int value, int max) {
    gfx_rect(g, x, y, w, h, GFX_WHITE);
    if (value < 0) value = 0;
    if (value > max) value = max;
    const int fill = (w - 4) * value / (max ? max : 1);
    if (fill > 0) gfx_fill_rect(g, x + 2, y + 2, fill, h - 4, GFX_WHITE);
}

int app_sin64(int a) {
    // A quarter wave in 17 steps, x256; the rest by symmetry.
    static const int16_t Q[17] = {0, 25, 50, 74, 98, 121, 142, 162, 181,
                                  198, 213, 226, 237, 245, 251, 255, 256};
    a &= 63;
    if (a < 16) return Q[a];
    if (a < 32) return Q[32 - a];
    if (a < 48) return -Q[a - 32];
    return -Q[64 - a];
}

void app_itoa(int v, char *out) {
    char tmp[12];
    int n = 0;
    const bool neg = v < 0;
    unsigned u = neg ? (unsigned)(-v) : (unsigned)v;
    do {
        tmp[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u && n < 11);
    int i = 0;
    if (neg) out[i++] = '-';
    while (n) out[i++] = tmp[--n];
    out[i] = 0;
}

void app_mini_face(gfx_t *g, int cx, int cy, int spread, int r, gfx_color_t c) {
    // The EMO face, small: two pill eyes, r wide and 2.5 r tall.
    const int w = 2 * r, h = 5 * r;
    gfx_fill_rrect(g, cx - spread - r, cy - h / 2, w, h, r, c);
    gfx_fill_rrect(g, cx + spread - r, cy - h / 2, w, h, r, c);
}

void app_heart(gfx_t *g, int cx, int cy, int s) {
    const int r = s / 4 + 1;
    gfx_fill_circle(g, cx - s / 4, cy - s / 6, r, GFX_WHITE);
    gfx_fill_circle(g, cx + s / 4, cy - s / 6, r, GFX_WHITE);
    gfx_fill_triangle(g, cx - s / 2, cy - s / 10, cx + s / 2, cy - s / 10, cx, cy + s / 2,
                      GFX_WHITE);
}

// ---------------------------------------------------------------------------
// Menu icons, about 28 px, centred on (cx, cy)
// ---------------------------------------------------------------------------

static void pip(gfx_t *g, int x, int y) {
    gfx_fill_circle(g, x, y, 2, GFX_BLACK);
}

static void fish(gfx_t *g, int cx, int cy) {
    gfx_fill_ellipse(g, cx - 2, cy, 10, 7, GFX_WHITE);
    gfx_fill_triangle(g, cx + 6, cy, cx + 14, cy - 8, cx + 14, cy + 8, GFX_WHITE);
    gfx_fill_circle(g, cx - 7, cy - 2, 1, GFX_BLACK);
    gfx_line(g, cx - 1, cy - 5, cx + 1, cy + 5, GFX_BLACK);
}

static void icon(gfx_t *g, app_item_t it, int cx, int cy, bool muted) {
    switch (it) {
        case ITEM_FEED:
            fish(g, cx, cy);
            break;
        case ITEM_GAMES:  // a gamepad
            gfx_fill_rrect(g, cx - 15, cy - 8, 30, 16, 7, GFX_WHITE);
            gfx_fill_rect(g, cx - 10, cy - 1, 7, 2, GFX_BLACK);
            gfx_fill_rect(g, cx - 8, cy - 3, 2, 7, GFX_BLACK);
            pip(g, cx + 6, cy + 1);
            pip(g, cx + 10, cy - 2);
            break;
        case ITEM_STATS:  // three bars and a heart
            gfx_fill_rect(g, cx - 12, cy + 2, 6, 10, GFX_WHITE);
            gfx_fill_rect(g, cx - 3, cy - 4, 6, 16, GFX_WHITE);
            gfx_fill_rect(g, cx + 6, cy - 1, 6, 13, GFX_WHITE);
            app_heart(g, cx, cy - 10, 8);
            break;
        case ITEM_SLEEP:  // crescent moon and a z
            gfx_fill_circle(g, cx - 3, cy, 12, GFX_WHITE);
            gfx_fill_circle(g, cx + 3, cy - 4, 11, GFX_BLACK);
            gfx_hline(g, cx + 6, cx + 12, cy + 4, GFX_WHITE);
            gfx_line(g, cx + 12, cy + 4, cx + 6, cy + 10, GFX_WHITE);
            gfx_hline(g, cx + 6, cx + 12, cy + 10, GFX_WHITE);
            break;
        case ITEM_SETUP:  // a spirit level
            gfx_fill_rrect(g, cx - 15, cy - 6, 30, 12, 5, GFX_WHITE);
            gfx_fill_rrect(g, cx - 12, cy - 3, 24, 6, 3, GFX_BLACK);
            gfx_fill_circle(g, cx, cy, 2, GFX_WHITE);
            gfx_fill_rect(g, cx - 5, cy - 3, 1, 6, GFX_WHITE);
            gfx_fill_rect(g, cx + 5, cy - 3, 1, 6, GFX_WHITE);
            break;
        case ITEM_DICE:
            gfx_fill_rrect(g, cx - 13, cy - 13, 26, 26, 5, GFX_WHITE);
            pip(g, cx - 6, cy - 6);
            pip(g, cx + 6, cy - 6);
            pip(g, cx, cy);
            pip(g, cx - 6, cy + 6);
            pip(g, cx + 6, cy + 6);
            break;
        case ITEM_COIN:
            gfx_fill_circle(g, cx, cy, 13, GFX_WHITE);
            gfx_fill_circle(g, cx, cy, 11, GFX_BLACK);
            app_mini_face(g, cx, cy - 1, 5, 2, GFX_WHITE);
            break;
        case ITEM_REACTION:  // lightning bolt
            gfx_fill_triangle(g, cx + 4, cy - 14, cx - 8, cy + 2, cx + 1, cy + 2, GFX_WHITE);
            gfx_fill_triangle(g, cx - 4, cy + 14, cx + 8, cy - 2, cx - 1, cy - 2, GFX_WHITE);
            break;
        case ITEM_CATCH:  // snowflake
            for (int k = -1; k <= 1; k++) {
                gfx_line(g, cx - 12, cy + k, cx + 12, cy + k, GFX_WHITE);
                gfx_line(g, cx - 6 + k, cy - 11, cx + 6 + k, cy + 11, GFX_WHITE);
                gfx_line(g, cx + 6 + k, cy - 11, cx - 6 + k, cy + 11, GFX_WHITE);
            }
            gfx_fill_circle(g, cx, cy, 3, GFX_WHITE);
            break;
        case ITEM_FLAPPY:  // a little round yeti with a wing
            gfx_fill_circle(g, cx + 2, cy, 11, GFX_WHITE);
            gfx_fill_circle(g, cx - 1, cy - 2, 2, GFX_BLACK);
            gfx_fill_circle(g, cx + 7, cy - 2, 2, GFX_BLACK);
            gfx_fill_rect(g, cx + 1, cy + 4, 4, 2, GFX_BLACK);
            gfx_fill_triangle(g, cx - 14, cy - 6, cx - 6, cy + 1, cx - 14, cy + 6, GFX_WHITE);
            break;
        case ITEM_SNAKE: {  // an S of blocks
            static const int8_t S[][2] = {{2, 0}, {1, 0}, {0, 0}, {0, 1}, {0, 2}, {1, 2},
                                          {2, 2}, {2, 3}, {2, 4}, {1, 4}, {0, 4}};
            for (unsigned i = 0; i < sizeof(S) / sizeof(S[0]); i++) {
                gfx_fill_rect(g, cx - 9 + S[i][0] * 6, cy - 14 + S[i][1] * 6, 5, 5, GFX_WHITE);
            }
            gfx_pixel(g, cx + 5, cy - 13, GFX_BLACK);
            break;
        }
        case ITEM_MUSIC:  // two quavers
            gfx_fill_circle(g, cx - 8, cy + 8, 4, GFX_WHITE);
            gfx_fill_circle(g, cx + 8, cy + 5, 4, GFX_WHITE);
            gfx_fill_rect(g, cx - 5, cy - 10, 2, 18, GFX_WHITE);
            gfx_fill_rect(g, cx + 11, cy - 13, 2, 18, GFX_WHITE);
            gfx_fill_triangle(g, cx - 5, cy - 10, cx + 13, cy - 13, cx + 13, cy - 9, GFX_WHITE);
            gfx_fill_triangle(g, cx - 5, cy - 10, cx - 5, cy - 6, cx + 13, cy - 9, GFX_WHITE);
            break;
        case ITEM_MEMORY:  // two eyes, one lit
            gfx_fill_circle(g, cx - 8, cy, 7, GFX_WHITE);
            gfx_fill_circle(g, cx + 8, cy, 7, GFX_WHITE);
            gfx_fill_circle(g, cx + 8, cy, 5, GFX_BLACK);
            break;
        case ITEM_SOUND:
            gfx_fill_rect(g, cx - 12, cy - 4, 6, 9, GFX_WHITE);
            gfx_fill_triangle(g, cx - 7, cy, cx + 1, cy - 10, cx + 1, cy + 10, GFX_WHITE);
            gfx_fill_rect(g, cx - 7, cy - 4, 8, 9, GFX_WHITE);
            if (muted) {
                for (int k = 0; k < 2; k++) {
                    gfx_line(g, cx + 5 + k, cy - 5, cx + 13 + k, cy + 5, GFX_WHITE);
                    gfx_line(g, cx + 13 + k, cy - 5, cx + 5 + k, cy + 5, GFX_WHITE);
                }
            } else {
                for (int k = 0; k < 2; k++) {
                    gfx_line(g, cx + 5 + k, cy - 4, cx + 5 + k, cy + 4, GFX_WHITE);
                    gfx_line(g, cx + 10 + k, cy - 8, cx + 10 + k, cy + 8, GFX_WHITE);
                }
            }
            break;
        case ITEM_UP:  // arrow back
            gfx_fill_triangle(g, cx - 14, cy, cx - 2, cy - 10, cx - 2, cy + 10, GFX_WHITE);
            gfx_fill_rect(g, cx - 3, cy - 3, 16, 7, GFX_WHITE);
            break;
        case ITEM_BACK:
            app_mini_face(g, cx, cy, 10, 4, GFX_WHITE);
            break;
        case ITEM_COUNT:
            break;
    }
}

static bool item_available(const app_t *a, app_item_t it) {
    if (it == ITEM_CATCH || it == ITEM_SETUP) return a->has_tilt;  // need the accelerometer
    return true;
}

static app_item_t list_item(const app_t *a, int i) {
    const int n = LISTS[a->list].n;
    return LISTS[a->list].items[((i % n) + n) % n];
}

static int list_pos(const app_t *a, app_item_t it) {
    for (int i = 0; i < LISTS[a->list].n; i++) {
        if (LISTS[a->list].items[i] == it) return i;
    }
    return 0;
}

static app_item_t next_item(const app_t *a, app_item_t it) {
    int i = list_pos(a, it);
    do {
        it = list_item(a, ++i);
    } while (!item_available(a, it));
    return it;
}

static void draw_item(const app_t *a, gfx_t *g, app_item_t it, int ox) {
    gfx_set_offset(g, ox, 0);
    icon(g, it, 64, 20, a->save.muted);
    const char *label = it == ITEM_SOUND ? (a->save.muted ? "SOUND OFF" : "SOUND ON") : app_item_name(it);
    font_draw_centered(g, 64, 39, label, 2, GFX_WHITE);
    gfx_set_offset(g, 0, 0);
}

static void render_menu(const app_t *a, gfx_t *g) {
    if (a->sliding) {
        // Ease out: fast at first, settling at the end.
        const int t = (int)(a->now - a->slide_t0);
        const int p = t >= SLIDE_MS ? 256 : 256 - (256 - t * 256 / SLIDE_MS) * (256 - t * 256 / SLIDE_MS) / 256;
        draw_item(a, g, a->slide_from, -128 * p / 256);
        draw_item(a, g, a->item, 128 - 128 * p / 256);
    } else {
        draw_item(a, g, a->item, 0);
    }
    // The hint line, with page dots between: where we are in this list.
    app_hint(g, "A:OK", "B:NEXT");
    int n = 0;
    for (int i = 0; i < LISTS[a->list].n; i++) n += item_available(a, list_item(a, i));
    int x = 58 - (n - 1) * 5 / 2;
    for (int i = 0; i < LISTS[a->list].n; i++) {
        const app_item_t it = list_item(a, i);
        if (!item_available(a, it)) continue;
        if (it == a->item) {
            gfx_fill_circle(g, x, 60, 2, GFX_WHITE);
        } else {
            gfx_pixel(g, x, 60, GFX_WHITE);
        }
        x += 5;
    }
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

void app_init(app_t *a, const app_env_t *env, const app_save_t *saved, uint32_t seed) {
    *a = (app_t){0};
    a->env = env;
    if (saved) a->save = *saved;
    if (a->save.pet.level == 0) pet_init(&a->save.pet);  // first boot: a new pet
    rng_seed(&a->rng, seed);
    a->mood = -1;
    a->item = ITEM_FEED;
}

bool app_active(const app_t *a) {
    return a->mode != APP_OFF;
}

static void slide_to(app_t *a, app_item_t it, uint32_t now) {
    a->slide_from = a->item;
    a->item = it;
    a->slide_t0 = now;
    a->sliding = true;
}

void app_open(app_t *a, uint32_t now) {
    int mg;
    a->has_tilt = a->env && a->env->tilt && a->env->tilt(&mg);
    a->mode = APP_MENU;
    a->list = 0;
    a->item = ITEM_FEED;
    a->sliding = false;
    a->now = a->last_input = now;
    app_play(a, SND_SELECT);
}

void app_close(app_t *a) {
    if (a->mode == APP_GAME && a->item == ITEM_MUSIC) music_leave(a);
    a->mode = APP_OFF;
}

int app_take_mood(app_t *a) {
    const int m = a->mood;
    a->mood = -1;
    return m;
}

bool app_take_sleep(app_t *a) {
    const bool s = a->want_sleep;
    a->want_sleep = false;
    return s;
}

#define EACH_SCREEN(X)                                                             \
    X(ITEM_DICE, dice) X(ITEM_COIN, coin) X(ITEM_REACTION, reaction)               \
    X(ITEM_CATCH, catch) X(ITEM_FLAPPY, flappy) X(ITEM_SNAKE, snake)               \
    X(ITEM_MEMORY, memory) X(ITEM_FEED, feed) X(ITEM_STATS, stats) X(ITEM_SETUP, setup)  \
    X(ITEM_MUSIC, music)

static void start_game(app_t *a, uint32_t now) {
    memset(&a->g, 0, sizeof(a->g));
    switch (a->item) {
#define X(it, name) \
    case it: name##_start(a, now); break;
        EACH_SCREEN(X)
#undef X
        default: return;
    }
    a->mode = APP_GAME;
}

static bool game_event(app_t *a, input_event_t ev, uint32_t now) {
    switch (a->item) {
#define X(it, name) \
    case it: return name##_event(a, ev, now);
        EACH_SCREEN(X)
#undef X
        default: return false;
    }
}

static void menu_choose(app_t *a, uint32_t now) {
    switch (a->item) {
        case ITEM_BACK:
            app_play(a, SND_BACK);
            app_close(a);
            break;
        case ITEM_GAMES:
            app_play(a, SND_SELECT);
            a->list = 1;
            slide_to(a, ITEM_DICE, now);
            break;
        case ITEM_UP:
            app_play(a, SND_BACK);
            a->list = 0;
            slide_to(a, ITEM_GAMES, now);
            break;
        case ITEM_SLEEP:
            a->want_sleep = true;
            app_close(a);
            break;
        case ITEM_SOUND:
            a->save.muted = !a->save.muted;
            if (a->env && a->env->mute) a->env->mute(a->save.muted);
            save(a);
            app_play(a, SND_SELECT);  // silent when just muted, which is the point
            break;
        default:
            app_play(a, SND_SELECT);
            start_game(a, now);
            break;
    }
}

static void menu_event(app_t *a, input_event_t ev, uint32_t now) {
    switch (ev) {
        case INPUT_BTN_B_SHORT:
            slide_to(a, next_item(a, a->item), now);
            app_play(a, SND_MOVE);
            break;
        case INPUT_BTN_A_SHORT:
            menu_choose(a, now);
            break;
        case INPUT_BTN_B_LONG:
            app_play(a, SND_BACK);
            app_close(a);
            break;
        default:
            break;
    }
}

void app_on_event(app_t *a, input_event_t ev, uint32_t now) {
    a->now = now;
    a->last_input = now;
    if (a->mode == APP_MENU) {
        menu_event(a, ev, now);
        return;
    }
    if (a->mode != APP_GAME) return;
    if (ev == INPUT_BTN_B_LONG) {
        app_play(a, SND_BACK);
        app_close(a);
        return;
    }
    if (game_event(a, ev, now)) return;
    if (ev == INPUT_BTN_B_SHORT) {  // anything the game did not take: B is back
        if (a->item == ITEM_MUSIC) music_leave(a);
        app_play(a, SND_BACK);
        a->mode = APP_MENU;
        a->sliding = false;
    }
}

// Being played right now, as opposed to a title or result screen that merely
// moves (Catch's Yeti follows the tilt on its title, Flappy's bird bobs).
static bool in_play(const app_t *a) {
    if (a->item == ITEM_CATCH) return a->g.ctch.phase == CATCH_PLAY;
    if (a->item == ITEM_FLAPPY) return a->g.fl.phase == FLAPPY_PLAY;
    if (a->item == ITEM_SNAKE) return a->g.snake.phase == SNAKE_PLAY;
    if (a->item == ITEM_SETUP) return false;  // its level bubble always moves
    if (a->item == ITEM_MUSIC) return a->g.music.playing;
    return app_is_animating(a);
}

void app_update(app_t *a, uint32_t now) {
    a->now = now;
    if (a->sliding && app_elapsed(now, a->slide_t0, SLIDE_MS)) a->sliding = false;
    if (a->levelup && app_elapsed(now, a->levelup_t0, LEVELUP_MS)) a->levelup = false;
    switch (a->mode) {
        case APP_MENU:
            if (app_elapsed(now, a->last_input, APP_MENU_IDLE_MS)) app_close(a);
            break;
        case APP_GAME:
            switch (a->item) {
#define X(it, name) \
    case it: name##_update(a, now); break;
                EACH_SCREEN(X)
#undef X
                default: break;
            }
            // A game in play (Catch is played by tilting, not pressing) keeps
            // the app open; any other screen times out, title screens included.
            if (!in_play(a) && app_elapsed(now, a->last_input, APP_GAME_IDLE_MS)) app_close(a);
            break;
        case APP_OFF:
            break;
    }
}

static void render_levelup(const app_t *a, gfx_t *g) {
    // A banner sliding down over whatever is showing.
    const int t = (int)(a->now - a->levelup_t0);
    const int y = t < 150 ? -18 + t * 18 / 150 : 0;
    gfx_fill_rect(g, 8, y, 112, 18, GFX_BLACK);
    gfx_fill_rrect(g, 9, y + 1, 110, 16, 4, GFX_WHITE);
    char line[16] = "LEVEL ";
    app_itoa(a->save.pet.level, line + 6);
    font_draw_centered(g, 64, y + 5, line, 1, GFX_BLACK);
    gfx_fill_rect(g, 20, y + 7, 3, 3, (t / 100) % 2 ? GFX_BLACK : GFX_WHITE);
    gfx_fill_rect(g, 105, y + 7, 3, 3, (t / 100) % 2 ? GFX_WHITE : GFX_BLACK);
}

void app_render(const app_t *a, gfx_t *g) {
    switch (a->mode) {
        case APP_MENU: render_menu(a, g); break;
        case APP_GAME:
            switch (a->item) {
#define X(it, name) \
    case it: name##_render(a, g); break;
                EACH_SCREEN(X)
#undef X
                default: break;
            }
            break;
        case APP_OFF: return;
    }
    if (a->levelup) render_levelup(a, g);
}

bool app_is_animating(const app_t *a) {
    if (a->levelup) return true;
    if (a->mode == APP_MENU) return a->sliding;
    if (a->mode != APP_GAME) return false;
    switch (a->item) {
#define X(it, name) \
    case it: return name##_animating(a);
        EACH_SCREEN(X)
#undef X
        default: return false;
    }
}

// ---------------------------------------------------------------------------
// The pet's clock and the face's interactions
// ---------------------------------------------------------------------------

void app_tick(app_t *a, uint32_t now, bool asleep) {
    if (a->pet_last == 0) a->pet_last = now;
    uint32_t dt = now - a->pet_last;
    a->pet_last = now;
    if (dt > 10 * PET_MINUTE_MS) dt = 10 * PET_MINUTE_MS;  // a stall: don't fast-forward
    a->pet_acc += dt;
    while (a->pet_acc >= PET_MINUTE_MS) {
        a->pet_acc -= PET_MINUTE_MS;
        pet_minute(&a->save.pet, asleep);
        if (++a->pet_unsaved >= PET_SAVE_EVERY) {
            a->pet_unsaved = 0;
            save(a);
        }
    }
}

void app_face_event(app_t *a, input_event_t ev) {
    switch (ev) {
        case INPUT_BTN_A_SHORT: pet_play(&a->save.pet, 2, 0); break;  // petting
        case INPUT_TAP: pet_play(&a->save.pet, 1, 0); break;
        case INPUT_DOUBLE_TAP: pet_play(&a->save.pet, 3, 0); break;
        case INPUT_SHAKE: pet_play(&a->save.pet, -2, 1); break;  // it didn't enjoy that
        default: break;
    }
}

uint8_t app_needs(const app_t *a) {
    return pet_needs(&a->save.pet);
}
