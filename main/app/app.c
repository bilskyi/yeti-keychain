#include "app/app_internal.h"

#include "app/art.h"

#include <stddef.h>
#include <string.h>

#define SLIDE_MS 180

static const char *const ITEM_NAMES[ITEM_COUNT] = {
    "DICE", "REACTION", "CATCH", "MAZE", "FLAPPY", "SNAKE", "DINO ROAR", "BIT", "PONG",
    "STATUS", "VOLUME", "BRIGHTNESS", "CALIBRATE", "UPDATE", "MUSIC", "67",
};

const char *app_item_name(app_item_t it) {
    return (unsigned)it < ITEM_COUNT ? ITEM_NAMES[it] : "?";
}

// In the order Sasha set in the Yeti Running Order artifact
// (https://claude.ai/artifact/FzG9RZ1VHmfEmNqoHkGEmB, db `order/main`).
// BIT's two come last, after that order.
static const app_item_t GAMES[] = {ITEM_CATCH, ITEM_MAZE, ITEM_FLAPPY, ITEM_SNAKE, ITEM_VOICE, ITEM_REACTION, ITEM_DICE,
                                   ITEM_PONG, ITEM_BIT};
static const app_item_t SETTINGS[] = {ITEM_STATUS, ITEM_VOLUME, ITEM_BRIGHTNESS, ITEM_SETUP, ITEM_UPDATE};
static const struct {
    const app_item_t *items;
    int n;
} LISTS[2] = {
    {GAMES, (int)(sizeof(GAMES) / sizeof(GAMES[0]))},
    {SETTINGS, (int)(sizeof(SETTINGS) / sizeof(SETTINGS[0]))},
};

static const char *const VOLUME_NAMES[APP_VOLUME_LEVELS] = {"SOUND OFF", "QUIET", "MEDIUM", "LOUD"};
static const char *const BRIGHTNESS_NAMES[APP_BRIGHTNESS_LEVELS] = {"DIM", "NORMAL", "BRIGHT"};

uint8_t app_contrast(int brightness) {
    static const uint8_t C[APP_BRIGHTNESS_LEVELS] = {0x08, 0x30, 0x90};
    if (brightness < 0) brightness = 0;
    if (brightness >= APP_BRIGHTNESS_LEVELS) brightness = APP_BRIGHTNESS_LEVELS - 1;
    return C[brightness];
}

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
    a->mood = EXPR_EXCITED;
    save(a);
    return true;
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
    app_hint(g, "AGAIN", NULL, "BACK");
}

void app_hint(gfx_t *g, const char *a_does, const char *b_does, const char *c_does) {
    if (a_does) font_draw(g, 1, 57, a_does, 1, GFX_WHITE);
    if (b_does) font_draw_centered(g, 64, 57, b_does, 1, GFX_WHITE);
    if (c_does) font_draw(g, GFX_W - 1 - font_width(c_does, 1), 57, c_does, 1, GFX_WHITE);
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
// Icons, about 28 px, centred on (cx, cy)
// ---------------------------------------------------------------------------

// A speaker with one wave per volume level, crossed out when off.
static void speaker(gfx_t *g, int cx, int cy, int level) {
    gfx_fill_rect(g, cx - 12, cy - 4, 6, 9, GFX_WHITE);
    gfx_fill_triangle(g, cx - 7, cy, cx + 1, cy - 10, cx + 1, cy + 10, GFX_WHITE);
    gfx_fill_rect(g, cx - 7, cy - 4, 8, 9, GFX_WHITE);
    if (level <= 0) {
        for (int k = 0; k < 2; k++) {
            gfx_line(g, cx + 5 + k, cy - 5, cx + 13 + k, cy + 5, GFX_WHITE);
            gfx_line(g, cx + 13 + k, cy - 5, cx + 5 + k, cy + 5, GFX_WHITE);
        }
        return;
    }
    for (int w = 0; w < level && w < 3; w++) {
        const int x = cx + 5 + w * 5, h = 3 + w * 3;
        for (int k = 0; k < 2; k++) gfx_line(g, x + k, cy - h, x + k, cy + h, GFX_WHITE);
    }
}

// A sun: longer rays the brighter.
static void sun(gfx_t *g, int cx, int cy, int level) {
    static const int8_t DIR[8][2] = {{10, 0}, {7, 7}, {0, 10}, {-7, 7}, {-10, 0}, {-7, -7}, {0, -10}, {7, -7}};
    gfx_fill_circle(g, cx, cy, 6, GFX_WHITE);
    const int len = 2 + level * 2;  // 2, 4, 6 px
    for (int i = 0; i < 8; i++) {
        const int x0 = cx + DIR[i][0] * 9 / 10, y0 = cy + DIR[i][1] * 9 / 10;
        const int x1 = cx + DIR[i][0] * (9 + len) / 10, y1 = cy + DIR[i][1] * (9 + len) / 10;
        gfx_line(g, x0, y0, x1, y1, GFX_WHITE);
        gfx_line(g, x0 + 1, y0, x1 + 1, y1, GFX_WHITE);  // 2 px thick
    }
}

// A battery: filled to pct, or with a bolt when on USB or charging (pct < 0).
static void battery(gfx_t *g, int cx, int cy, int pct) {
    gfx_rect(g, cx - 13, cy - 7, 24, 14, GFX_WHITE);
    gfx_fill_rect(g, cx + 11, cy - 3, 3, 6, GFX_WHITE);
    if (pct < 0) {
        gfx_fill_triangle(g, cx + 1, cy - 6, cx - 6, cy + 1, cx, cy + 1, GFX_WHITE);
        gfx_fill_triangle(g, cx - 2, cy + 6, cx + 4, cy - 1, cx - 1, cy - 1, GFX_WHITE);
        return;
    }
    const int w = 20 * (pct > 100 ? 100 : pct) / 100;
    if (w > 0) gfx_fill_rect(g, cx - 11, cy - 5, w, 10, GFX_WHITE);
}

static void quavers(gfx_t *g, int cx, int cy) {
    gfx_fill_circle(g, cx - 8, cy + 8, 4, GFX_WHITE);
    gfx_fill_circle(g, cx + 8, cy + 5, 4, GFX_WHITE);
    gfx_fill_rect(g, cx - 5, cy - 10, 2, 18, GFX_WHITE);
    gfx_fill_rect(g, cx + 11, cy - 13, 2, 18, GFX_WHITE);
    gfx_fill_triangle(g, cx - 5, cy - 10, cx + 13, cy - 13, cx + 13, cy - 9, GFX_WHITE);
    gfx_fill_triangle(g, cx - 5, cy - 10, cx - 5, cy - 6, cx + 13, cy - 9, GFX_WHITE);
}

static void icon(const app_t *a, gfx_t *g, app_item_t it, int cx, int cy) {
    switch (it) {
        case ITEM_MUSIC:
            quavers(g, cx, cy);
            break;
        case ITEM_EGG:  // never in a list
            break;
        case ITEM_STATUS:  // drawn whole by draw_item
            break;
        case ITEM_VOLUME:
            speaker(g, cx, cy, a->save.volume);
            break;
        case ITEM_BRIGHTNESS:
            sun(g, cx, cy, a->save.brightness);
            break;
        case ITEM_UPDATE:  // an arrow down into a chip
            gfx_fill_rect(g, cx - 3, cy - 14, 6, 10, GFX_WHITE);
            gfx_fill_triangle(g, cx - 8, cy - 5, cx + 8, cy - 5, cx, cy + 3, GFX_WHITE);
            gfx_fill_rrect(g, cx - 12, cy + 5, 24, 9, 2, GFX_WHITE);
            for (int k = -9; k <= 9; k += 6) {
                gfx_fill_rect(g, cx + k, cy + 14, 2, 2, GFX_WHITE);
                gfx_fill_rect(g, cx + k, cy + 3, 2, 2, GFX_WHITE);
            }
            break;
        case ITEM_SETUP:  // a spirit level
            gfx_fill_rrect(g, cx - 15, cy - 6, 30, 12, 5, GFX_WHITE);
            gfx_fill_rrect(g, cx - 12, cy - 3, 24, 6, 3, GFX_BLACK);
            gfx_fill_circle(g, cx, cy, 2, GFX_WHITE);
            gfx_fill_rect(g, cx - 5, cy - 3, 1, 6, GFX_WHITE);
            gfx_fill_rect(g, cx + 5, cy - 3, 1, 6, GFX_WHITE);
            break;
        default:  // the games have pictures of their own (art.c)
            break;
    }
}

// ---------------------------------------------------------------------------
// The menu
// ---------------------------------------------------------------------------

static bool item_available(const app_t *a, app_item_t it) {
    if (it == ITEM_CATCH || it == ITEM_MAZE || it == ITEM_SETUP) return a->has_tilt;  // need the accelerometer
    // BIT's two only while BIT is near: there is nobody to throw at otherwise.
    if (it == ITEM_BIT) return app_link_status(a) == APP_LINK_NEAR;
    if (it == ITEM_PONG) return app_link_status(a) == APP_LINK_NEAR && a->has_tilt;
    if (it == ITEM_UPDATE) return a->env && a->env->update_begin;
    return true;
}

static int list_of(app_item_t it) {
    for (int l = 1; l < 2; l++) {
        for (int i = 0; i < LISTS[l].n; i++) {
            if (LISTS[l].items[i] == it) return l;
        }
    }
    return 0;
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

static bool is_setting(app_item_t it) {
    return it == ITEM_VOLUME || it == ITEM_BRIGHTNESS;
}

static const char *item_label(const app_t *a, app_item_t it) {
    if (it == ITEM_VOLUME) return VOLUME_NAMES[a->save.volume % APP_VOLUME_LEVELS];
    if (it == ITEM_BRIGHTNESS) return BRIGHTNESS_NAMES[a->save.brightness % APP_BRIGHTNESS_LEVELS];
    return app_item_name(it);
}

static void draw_status(const app_t *a, gfx_t *g);

static void draw_item(const app_t *a, gfx_t *g, app_item_t it, int ox) {
    gfx_set_offset(g, ox, 0);
    if (it == ITEM_STATUS) {
        draw_status(a, g);
        gfx_set_offset(g, 0, 0);
        return;
    }
    if (list_of(it) == 0) {  // a game: its picture and its name, nothing more
        art_game(g, it);
        font_draw_centered(g, 64, 41, app_item_name(it), 2, GFX_WHITE);
    } else {
        icon(a, g, it, 64, 20);
        font_draw_centered(g, 64, 39, item_label(a, it), 2, GFX_WHITE);
    }
    gfx_set_offset(g, 0, 0);
}

static void render_menu(const app_t *a, gfx_t *g) {
    if (a->sliding) {
        // Ease out: fast at first, settling at the end. Forward slides come in
        // from the right, going back comes in from the left.
        const int t = (int)(a->now - a->slide_t0);
        const int q = t >= SLIDE_MS ? 0 : 256 - t * 256 / SLIDE_MS;
        const int p = 256 - q * q / 256;
        const int d = a->slide_dir < 0 ? -1 : 1;
        draw_item(a, g, a->slide_from, -d * 128 * p / 256);
        draw_item(a, g, a->item, d * (128 - 128 * p / 256));
    } else {
        draw_item(a, g, a->item, 0);
    }
    app_hint(g, a->item == ITEM_STATUS ? NULL : is_setting(a->item) ? "CHANGE" : "OPEN", "NEXT", "BACK");
    // Where we are in this list: a column of dots on the right.
    int n = 0;
    for (int i = 0; i < LISTS[a->list].n; i++) n += item_available(a, list_item(a, i));
    int y = 20 - (n - 1) * 3;
    for (int i = 0; i < LISTS[a->list].n; i++) {
        const app_item_t it = list_item(a, i);
        if (!item_available(a, it)) continue;
        if (it == a->item) {
            gfx_fill_rect(g, 123, y - 1, 3, 3, GFX_WHITE);
        } else {
            gfx_pixel(g, 124, y, GFX_WHITE);
        }
        y += 6;
    }
}

// The status card (C on the face, first in its list): volume, brightness,
// battery, at a glance.
static void draw_status(const app_t *a, gfx_t *g) {
    int pct = -1;
    const bool on_battery = a->env && a->env->battery && a->env->battery(&pct);
    const bool charging = a->env && a->env->charging && a->env->charging();
    speaker(g, 22, 20, a->save.volume);
    sun(g, 64, 20, a->save.brightness);
    battery(g, 106, 20, on_battery && !charging ? pct : -1);
    static const char *const VOL[APP_VOLUME_LEVELS] = {"OFF", "QUIET", "MEDIUM", "LOUD"};
    font_draw_centered(g, 22, 40, VOL[a->save.volume % APP_VOLUME_LEVELS], 1, GFX_WHITE);
    font_draw_centered(g, 64, 40, BRIGHTNESS_NAMES[a->save.brightness % APP_BRIGHTNESS_LEVELS], 1, GFX_WHITE);
    char b[8] = "USB";
    if (charging) strcpy(b, "CHG");
    if (on_battery) {
        app_itoa(pct, b);
        const size_t n = strlen(b);
        b[n] = '%';
        b[n + 1] = 0;
    }
    font_draw_centered(g, 106, 40, b, 1, GFX_WHITE);
    // The reading itself, small, as "4.12V".
    const int mv = a->env && a->env->battery_mv ? a->env->battery_mv() : -1;
    if (mv >= 0) {
        app_itoa(mv / 1000, b);
        size_t n = strlen(b);
        b[n++] = '.';
        b[n++] = (char)('0' + mv / 100 % 10);
        b[n++] = (char)('0' + mv / 10 % 10);
        b[n++] = 'V';
        b[n] = 0;
        font_draw_centered(g, 106, 48, b, 1, GFX_WHITE);
    }
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

void app_init(app_t *a, const app_env_t *env, const app_save_t *saved, uint32_t seed) {
    *a = (app_t){0};
    a->env = env;
    if (saved) a->save = *saved;
    if (a->save.volume >= APP_VOLUME_LEVELS) a->save.volume = APP_VOLUME_LEVELS - 1;
    if (a->save.brightness >= APP_BRIGHTNESS_LEVELS) a->save.brightness = APP_BRIGHTNESS_LEVELS - 1;
    rng_seed(&a->rng, seed);
    a->mood = -1;
    a->item = a->last_game = GAMES[0];
    a->last_viz = -1;
}

bool app_active(const app_t *a) {
    return a->mode != APP_OFF;
}

static void slide_to(app_t *a, app_item_t it, int dir, uint32_t now) {
    a->slide_from = a->item;
    a->item = it;
    a->slide_t0 = now;
    a->slide_dir = dir;
    a->sliding = true;
}

static void detect_tilt(app_t *a) {
    int mg;
    a->has_tilt = a->env && a->env->tilt && a->env->tilt(&mg);
}

static void open_list(app_t *a, int list, app_item_t it, uint32_t now) {
    if (a->mode == APP_GAME && a->item == ITEM_MUSIC) music_leave(a);
    detect_tilt(a);
    a->mode = APP_MENU;
    a->list = list;
    a->item = it;
    a->sliding = false;
    a->now = a->last_input = now;
    app_play(a, SND_SELECT);
}

void app_open_games(app_t *a, uint32_t now) {
    open_list(a, 0, a->last_game, now);
    if (!item_available(a, a->item)) a->item = next_item(a, a->item);
}

void app_open_settings(app_t *a, uint32_t now) {
    open_list(a, 1, ITEM_STATUS, now);
}

static void start_game(app_t *a, uint32_t now);

void app_open_music(app_t *a, uint32_t now) {
    if (a->mode == APP_GAME && a->item == ITEM_MUSIC) return;
    if (a->mode == APP_GAME) app_close(a);
    a->item = ITEM_MUSIC;
    a->now = a->last_input = now;
    start_game(a, now);
}

#define EGG_TAPS 6
#define EGG_TAP_GAP_MS 900   // presses further apart start the count again
#define EGG_SHAKE_MS 3000    // after the sixth, this long to shake it

bool app_egg_watch(app_t *a, input_event_t ev, uint32_t now) {
    switch (ev) {
        case INPUT_BTN_A_SHORT:
            if (a->egg_taps && app_elapsed(now, a->egg_at, EGG_TAP_GAP_MS)) a->egg_taps = 0;
            a->egg_taps++;
            a->egg_at = now;
            return false;
        case INPUT_SHAKE:
            if (a->egg_taps < EGG_TAPS || app_elapsed(now, a->egg_at, EGG_SHAKE_MS)) {
                a->egg_taps = 0;
                return false;
            }
            a->egg_taps = 0;
            if (a->mode == APP_GAME) app_close(a);
            a->item = ITEM_EGG;
            a->now = a->last_input = now;
            start_game(a, now);
            return true;
        case INPUT_BTN_B_SHORT:
        case INPUT_BTN_B_LONG:
        case INPUT_BTN_C_SHORT:
        case INPUT_BTN_C_LONG:
        case INPUT_BTN_A_LONG:
            a->egg_taps = 0;
            return false;
        default:
            return false;
    }
}

void app_close(app_t *a) {
    if (a->mode == APP_GAME && a->item == ITEM_MUSIC) music_leave(a);
    if (a->mode == APP_GAME && a->item == ITEM_PONG) pongk_leave(a);
    if (a->mode == APP_GAME && a->item == ITEM_UPDATE) update_leave(a);
    a->mode = APP_OFF;
}

int app_take_mood(app_t *a) {
    const int m = a->mood;
    a->mood = -1;
    return m;
}

#define EACH_SCREEN(X)                                                          \
    X(ITEM_DICE, dice) X(ITEM_REACTION, reaction) X(ITEM_CATCH, catch) X(ITEM_MAZE, maze) \
    X(ITEM_FLAPPY, flappy) X(ITEM_SNAKE, snake) X(ITEM_VOICE, voice)            \
    X(ITEM_BIT, toss) X(ITEM_PONG, pongk)                                       \
    X(ITEM_SETUP, setup) X(ITEM_UPDATE, update) X(ITEM_MUSIC, music) X(ITEM_EGG, egg)

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

static void change_setting(app_t *a) {
    if (a->item == ITEM_VOLUME) {
        a->save.volume = (uint8_t)((a->save.volume + 1) % APP_VOLUME_LEVELS);
        if (a->env && a->env->volume) a->env->volume(a->save.volume);
    } else {
        a->save.brightness = (uint8_t)((a->save.brightness + 1) % APP_BRIGHTNESS_LEVELS);
        if (a->env && a->env->brightness) a->env->brightness(a->save.brightness);
    }
    save(a);
    app_play(a, SND_SELECT);  // at the new volume: a sample of it (silent when off)
}

static void menu_choose(app_t *a, uint32_t now) {
    switch (a->item) {
        case ITEM_STATUS:
            break;  // nothing to choose: B goes on to the settings
        case ITEM_VOLUME:
        case ITEM_BRIGHTNESS:
            change_setting(a);
            break;
        default:
            app_play(a, SND_SELECT);
            if (a->list == 0) a->last_game = a->item;
            start_game(a, now);
            break;
    }
}

static void menu_event(app_t *a, input_event_t ev, uint32_t now) {
    switch (ev) {
        case INPUT_BTN_A_SHORT:
            menu_choose(a, now);
            break;
        case INPUT_BTN_B_SHORT:
            slide_to(a, next_item(a, a->item), 1, now);
            app_play(a, SND_MOVE);
            break;
        case INPUT_BTN_B_LONG:  // held B: the music, from the lists too
            app_open_music(a, now);
            break;
        case INPUT_BTN_C_SHORT:
            app_play(a, SND_BACK);
            app_close(a);
            break;
        default:
            break;
    }
}

// From a game back to the list it was started from, with it still selected;
// from the music and the easter egg, back to the face.
static void leave_game(app_t *a) {
    app_play(a, SND_BACK);
    if (a->item == ITEM_MUSIC || a->item == ITEM_EGG) {
        app_close(a);
        return;
    }
    a->mode = APP_MENU;
    a->list = list_of(a->item);
    a->sliding = false;
}

void app_on_event(app_t *a, input_event_t ev, uint32_t now) {
    a->now = now;
    a->last_input = now;
    if (a->mode == APP_OFF) return;
    if (ev == INPUT_BTN_C_LONG) {  // held C: home, from anywhere
        app_play(a, SND_BACK);
        app_close(a);
        return;
    }
    switch (a->mode) {
        case APP_MENU:
            menu_event(a, ev, now);
            return;
        case APP_GAME:
            if (game_event(a, ev, now)) return;
            if (ev == INPUT_BTN_C_SHORT) leave_game(a);
            return;
        case APP_OFF:
            return;
    }
}

// Being played right now, as opposed to a title or result screen that merely
// moves (Catch's Yeti follows the tilt on its title, Flappy's bird bobs).
static bool in_play(const app_t *a) {
    if (a->item == ITEM_CATCH) return a->g.ctch.phase == CATCH_PLAY;
    if (a->item == ITEM_MAZE) return a->g.maze.phase == MAZE_PLAY || a->g.maze.phase == MAZE_SOLVED;
    if (a->item == ITEM_FLAPPY) return a->g.fl.phase == FLAPPY_PLAY;
    if (a->item == ITEM_SNAKE) return a->g.snake.phase == SNAKE_PLAY;
    if (a->item == ITEM_VOICE) return a->g.voice.phase == VOICE_PLAY;
    // Waiting on BIT counts as play: an answer can take a while to come.
    if (a->item == ITEM_BIT) return a->g.toss.phase != TOSS_AIM;
    if (a->item == ITEM_PONG) return a->g.pong.phase == PK_PLAY || a->g.pong.phase == PK_ASKING;
    if (a->item == ITEM_SETUP) return false;  // its level bubble always moves
    if (a->item == ITEM_UPDATE) return true;  // it closes itself after ten idle minutes
    if (a->item == ITEM_MUSIC) return a->g.music.playing;
    if (a->item == ITEM_EGG) return true;  // it ends itself
    return app_is_animating(a);
}

void app_update(app_t *a, uint32_t now) {
    a->now = now;
    if (a->sliding && app_elapsed(now, a->slide_t0, SLIDE_MS)) a->sliding = false;
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
            // A game in play (Catch and Maze are played by tilting, Voice by shouting)
            // keeps the app open; any other screen times out.
            if (!in_play(a) && app_elapsed(now, a->last_input, APP_GAME_IDLE_MS)) app_close(a);
            break;
        case APP_OFF:
            break;
    }
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
        case APP_OFF: break;
    }
}

bool app_is_animating(const app_t *a) {
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

bool app_wants_mic(const app_t *a) {
    return a->mode == APP_GAME && a->item == ITEM_VOICE;
}

bool app_wants_link(const app_t *a) {
    return a->mode == APP_GAME && (a->item == ITEM_BIT || a->item == ITEM_PONG);
}

void app_on_link(app_t *a, const bl_msg_t *m, uint32_t now) {
    if (a->mode != APP_GAME) return;
    a->now = now;
    if (a->item == ITEM_BIT) toss_link(a, m, now);
    if (a->item == ITEM_PONG) pongk_link(a, m, now);
}

void app_on_link_done(app_t *a, const bl_msg_t *m, bool delivered, uint32_t now) {
    if (a->mode != APP_GAME) return;
    a->now = now;
    if (a->item == ITEM_BIT) toss_link_done(a, m, delivered, now);
    if (a->item == ITEM_PONG) pongk_link_done(a, m, delivered, now);
}

bool app_link_send(const app_t *a, const bl_msg_t *m, bool reliable) {
    return a->env && a->env->link_send && a->env->link_send(m, reliable);
}

app_link_t app_link_status(const app_t *a) {
    if (!a->env || !a->env->link_status || !a->env->link_send) return APP_LINK_OFF;
    return (app_link_t)a->env->link_status();
}
