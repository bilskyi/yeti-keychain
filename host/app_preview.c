// Renders the menu, the status card and each game's screens to PBM files,
// which png_sheet.py lays out as one PNG, like face_preview does for the face.
// It is NOT a substitute for checking the real panel.

#include <stdio.h>
#include <string.h>

#include "app/app.h"
#include "display/gfx.h"

static void write_pbm(const char *dir, int idx, const char *name, const gfx_t *g) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%02d_%s.pbm", dir, idx, name);
    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror(path);
        return;
    }
    fprintf(fp, "P1\n%d %d\n", GFX_W, GFX_H);
    for (int y = 0; y < GFX_H; y++) {
        for (int x = 0; x < GFX_W; x++) fputc(gfx_get_pixel(g, x, y) ? '1' : '0', fp);
        fputc('\n', fp);
    }
    fclose(fp);
}

static bool tilt(int *mg) {
    *mg = 0;
    return true;
}
static bool tilt_xy(int *mg) {
    mg[0] = mg[1] = 0;
    return true;
}
static int loudness(void) { return 45; }
static bool battery(int *pct) {
    *pct = 72;
    return true;
}
static int battery_mv(void) { return 3912; }
static const app_env_t ENV = {.tilt = tilt, .tilt_xy = tilt_xy, .loudness = loudness,
                              .battery = battery, .battery_mv = battery_mv};

static void ev(app_t *a, input_event_t e, uint32_t *now) {
    app_on_event(a, e, *now);
    for (int i = 0; i < 10; i++) app_update(a, *now += 33);  // past the slide
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    static app_t a;
    static gfx_t g;
    uint32_t now = 1000;
    int n = 0;
    const app_save_t save = APP_SAVE_DEFAULTS;
    app_init(&a, &ENV, &save, 1);
#define SNAP(name)                         \
    do {                                   \
        gfx_clear(&g);                     \
        app_render(&a, &g);                \
        write_pbm(dir, n++, (name), &g);   \
    } while (0)

    // Every item of both lists: the games (B on the face), then the status
    // card and settings (C).
    for (int l = 0; l < 2; l++) {
        if (l) {
            app_open_settings(&a, now);
        } else {
            app_open_games(&a, now);
        }
        const app_item_t first = a.item;
        do {
            SNAP(app_item_name(a.item));
            ev(&a, INPUT_BTN_B_SHORT, &now);
        } while (a.item != first);
    }
    app_close(&a);

    // Each game's title screen, then a moment of play.
    static const app_item_t GAMES[] = {ITEM_DICE, ITEM_REACTION, ITEM_CATCH, ITEM_MAZE, ITEM_FLAPPY,
                                       ITEM_SNAKE, ITEM_VOICE, ITEM_MUSIC, ITEM_SETUP};
    for (unsigned k = 0; k < sizeof(GAMES) / sizeof(GAMES[0]); k++) {
        const app_item_t it = GAMES[k];
        if (it == ITEM_MUSIC) {
            app_open_music(&a, now);
        } else {
            if (it == ITEM_SETUP) {
                app_open_settings(&a, now);
            } else {
                app_open_games(&a, now);
            }
            while (a.item != it) ev(&a, INPUT_BTN_B_SHORT, &now);
            ev(&a, INPUT_BTN_A_SHORT, &now);
        }
        char name[32];
        snprintf(name, sizeof(name), "%s_title", app_item_name(it));
        SNAP(name);
        if (it == ITEM_SETUP) continue;
        ev(&a, INPUT_BTN_A_PRESS, &now);
        ev(&a, INPUT_BTN_A_SHORT, &now);
        if (it == ITEM_FLAPPY) app_on_event(&a, INPUT_BTN_A_PRESS, now);  // flap again, still flying
        for (int i = 0; i < 15; i++) app_update(&a, now += 33);
        snprintf(name, sizeof(name), "%s_play", app_item_name(it));
        SNAP(name);
        app_close(&a);
    }

    // The jukebox's list, each song with its picture.
    app_open_music(&a, now);
    for (int s = 0; s < songs_count(); s++) {
        char name[32];
        snprintf(name, sizeof(name), "SONG_%d", s + 1);
        SNAP(name);
        ev(&a, INPUT_BTN_B_SHORT, &now);
    }
    app_close(&a);

    // Each music animation, 4 s into Take On Me.
    static viz_track_t tr;
    static viz_t v;
    rng_t rng;
    rng_seed(&rng, 3);
    viz_track_load(&tr, songs_count() - 1);
    for (int k = 0; k < VIZ_COUNT; k++) {
        viz_start(&v, (viz_kind_t)k, &rng);
        int next = 0;
        uint32_t next_at = 0, t = 0;
        for (; t < 4000; t += 33) {
            while (next < tr.n && next_at <= t) {
                if (tr.notes[next].hz) viz_note(&v, &tr, next, &rng);
                next_at += tr.notes[next++].ms;
            }
            viz_step(&v, 33, &rng);
        }
        gfx_clear(&g);
        viz_draw(&v, &tr, t, &g);
        char name[32];
        snprintf(name, sizeof(name), "VIZ_%s", viz_name((viz_kind_t)k));
        write_pbm(dir, n++, name, &g);
    }

    // The 67 easter egg, a frame from each part of the show.
    for (int k = 0; k < 6; k++) app_egg_watch(&a, INPUT_BTN_A_SHORT, now += 200);
    app_egg_watch(&a, INPUT_SHAKE, now += 300);
    static const uint32_t AT[] = {400, 950, 1300, 1560, 2000, 3000, 3300, 5250, 5800, 6500};
    const uint32_t t0 = a.g.egg.t0;
    for (unsigned k = 0; k < sizeof(AT) / sizeof(AT[0]); k++) {
        a.now = t0 + AT[k];
        char name[32];
        snprintf(name, sizeof(name), "EGG_%u", (unsigned)AT[k]);
        SNAP(name);
    }
    return 0;
}
