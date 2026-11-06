// Host tests for the menu and the games, with a fake environment that
// records sounds and saves and lets a test set the tilt.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app.h"
#include "app/art.h"
#include "config.h"
#include "face/face.h"
#include "input/orient.h"
#include "pet/story.h"
#include "sound/rtttl.h"

#define FRAME 33

static int sounds[SND_COUNT];
static int saves;
static app_save_t saved;
static bool has_tilt;
static int tilt_mg;
static int tilt_xy[2];
static int raw_mg[3];
static int8_t applied[3];
static int orients;
static int song_on = -1;  // the song "playing", -1 = none
static bool muted_fake;
static int volume_set = -1, brightness_set = -1;
static int loudness_fake;  // -1 = no mic
static bool charging_fake;
// The radio to BIT: what was sent, and how things stand.
static bl_msg_t sent[32];
static bool sent_reliable[32];
static int nsent;
static int link_fake;  // an app_link_t
static uint32_t heard_fake;
// The update over WiFi.
static app_update_t upd_fake;
static int upd_begins, upd_ends;

static void fake_play(app_sound_t s) { sounds[s]++; }
static void fake_save(const app_save_t *s) {
    saves++;
    saved = *s;
}
static void fake_volume(int v) { volume_set = v; }
static void fake_brightness(int b) { brightness_set = b; }
static int fake_loudness(void) { return loudness_fake; }
static bool fake_charging(void) { return charging_fake; }
static int fake_battery_mv(void) { return 3912; }
static bool fake_tilt(int *mg) {
    *mg = tilt_mg;
    return has_tilt;
}
static bool fake_tilt_xy(int *mg) {
    mg[0] = tilt_xy[0];
    mg[1] = tilt_xy[1];
    return has_tilt;
}
static bool fake_accel(int *mg) {
    for (int i = 0; i < 3; i++) mg[i] = raw_mg[i];
    return has_tilt;
}
static void fake_orient(const int8_t *map) {
    for (int i = 0; i < 3; i++) applied[i] = map[i];
    orients++;
}
static bool fake_song(int i) {
    if (muted_fake) return false;
    song_on = i;
    return true;
}
static void fake_song_stop(void) { song_on = -1; }
static bool fake_song_playing(void) { return song_on >= 0; }
static bool fake_link_send(const bl_msg_t *m, bool reliable) {
    if (link_fake == APP_LINK_OFF) return false;
    if (nsent < 32) {
        sent[nsent] = *m;
        sent_reliable[nsent++] = reliable;
    }
    return true;
}
static int fake_link_status(void) { return link_fake; }
static uint32_t fake_heard_at(void) { return heard_fake; }
static bool fake_update_begin(void) {
    upd_begins++;
    upd_fake.phase = APP_UPD_WAITING;
    strcpy(upd_fake.ssid, "YETI-7");
    strcpy(upd_fake.pass, "12345678");
    strcpy(upd_fake.addr, "192.168.4.1");
    return true;
}
static void fake_update_end(void) {
    upd_ends++;
    upd_fake.phase = APP_UPD_OFF;
}
static void fake_update_status(app_update_t *u) { *u = upd_fake; }
static const app_env_t ENV = {
    .song = fake_song,
    .song_stop = fake_song_stop,
    .song_playing = fake_song_playing,
    .play = fake_play,
    .save = fake_save,
    .volume = fake_volume,
    .brightness = fake_brightness,
    .loudness = fake_loudness,
    .tilt = fake_tilt,
    .tilt_xy = fake_tilt_xy,
    .accel = fake_accel,
    .orient = fake_orient,
    .charging = fake_charging,
    .battery_mv = fake_battery_mv,
    .link_send = fake_link_send,
    .link_status = fake_link_status,
    .link_heard_at = fake_heard_at,
    .update_begin = fake_update_begin,
    .update_end = fake_update_end,
    .update_status = fake_update_status,
};

static void reset_fakes(void) {
    memset(sounds, 0, sizeof(sounds));
    saves = 0;
    memset(&saved, 0, sizeof(saved));
    has_tilt = true;
    tilt_mg = 0;
    tilt_xy[0] = tilt_xy[1] = 0;
    memset(raw_mg, 0, sizeof(raw_mg));
    memset(applied, 0, sizeof(applied));
    orients = 0;
    song_on = -1;
    muted_fake = false;
    charging_fake = false;
    volume_set = brightness_set = -1;
    loudness_fake = 0;
    nsent = 0;
    link_fake = APP_LINK_NEAR;
    heard_fake = 0;
    memset(&upd_fake, 0, sizeof(upd_fake));
    upd_begins = upd_ends = 0;
}

static void run(app_t *a, uint32_t ms, uint32_t *now) {
    for (uint32_t t = 0; t < ms; t += FRAME) app_update(a, *now += FRAME);
}

static void ev(app_t *a, input_event_t e, uint32_t *now) {
    app_on_event(a, e, *now);
    app_update(a, *now += FRAME);
}

// A tap of A as the buttons deliver it: PRESS, then SHORT on release.
static void press_a(app_t *a, uint32_t *now) {
    ev(a, INPUT_BTN_A_PRESS, now);
    ev(a, INPUT_BTN_A_SHORT, now);
}

static void press_b(app_t *a, uint32_t *now) {
    ev(a, INPUT_BTN_B_PRESS, now);
    ev(a, INPUT_BTN_B_SHORT, now);
}

// Opens `it` the way the buttons do: the games list (B on the face) or the
// status and settings list (C), B along to it, A. The music opens on its own,
// the easter egg with six presses of A and a shake.
static void open_item(app_t *a, app_item_t it, uint32_t *now) {
    if (it == ITEM_MUSIC) {
        app_open_music(a, *now);
    } else if (it == ITEM_EGG) {
        for (int k = 0; k < 6; k++) assert(!app_egg_watch(a, INPUT_BTN_A_SHORT, *now += 200));
        assert(app_egg_watch(a, INPUT_SHAKE, *now += 300));
    } else {
        if (it >= ITEM_STATUS) {
            app_open_settings(a, *now);
        } else {
            app_open_games(a, *now);
        }
        for (int k = 0; a->item != it; k++) {
            assert(k < ITEM_COUNT);
            ev(a, INPUT_BTN_B_SHORT, now);
        }
        ev(a, INPUT_BTN_A_SHORT, now);
    }
    assert(a->mode == APP_GAME && a->item == it);
}

static void test_menu(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 1);
    assert(!app_active(&a));

    // B on the face: straight into the games, no category to pick first.
    app_open_games(&a, now);
    assert(a.mode == APP_MENU && a.list == 0 && a.item == ITEM_CATCH);
    // B walks the seven games in Sasha's order, then BIT's two, and wraps; the
    // slide animates, then stops.
    static const app_item_t ORDER[] = {ITEM_MAZE, ITEM_FLAPPY,  ITEM_SNAKE, ITEM_VOICE, ITEM_REACTION,
                                       ITEM_DICE, ITEM_PONG, ITEM_BIT, ITEM_CATCH};
    for (int k = 0; k < 9; k++) {
        ev(&a, INPUT_BTN_B_SHORT, &now);
        assert(app_is_animating(&a));
        run(&a, 300, &now);
        assert(!app_is_animating(&a));
        assert(a.item == ORDER[k]);
    }
    // No radio, BIT never found, or BIT away: BIT's two are not in the list.
    static const int NOT_NEAR[] = {APP_LINK_OFF, APP_LINK_SEARCHING, APP_LINK_AWAY};
    for (int n = 0; n < 3; n++) {
        link_fake = NOT_NEAR[n];
        for (int k = 0; k < 7; k++) {
            ev(&a, INPUT_BTN_B_SHORT, &now);
            run(&a, 300, &now);
            assert(a.item != ITEM_PONG && a.item != ITEM_BIT);
        }
    }
    link_fake = APP_LINK_NEAR;
    // C closes it: one step from the face, one step back.
    ev(&a, INPUT_BTN_C_SHORT, &now);
    assert(!app_active(&a));

    // It opens again on the last game played.
    open_item(&a, ITEM_SNAKE, &now);
    ev(&a, INPUT_BTN_C_SHORT, &now);
    assert(a.mode == APP_MENU && a.list == 0 && a.item == ITEM_SNAKE);
    app_close(&a);
    app_open_games(&a, now);
    assert(a.item == ITEM_SNAKE);
    app_close(&a);

    // C on the face: the status card first, then the settings.
    app_open_settings(&a, now);
    assert(a.mode == APP_MENU && a.list == 1 && a.item == ITEM_STATUS);
    ev(&a, INPUT_BTN_A_SHORT, &now);  // nothing to do on the card
    assert(a.mode == APP_MENU && a.item == ITEM_STATUS && saves == 0);
    static const app_item_t SET[] = {ITEM_VOLUME, ITEM_BRIGHTNESS, ITEM_SETUP, ITEM_UPDATE, ITEM_STATUS};
    for (int k = 0; k < 5; k++) {
        ev(&a, INPUT_BTN_B_SHORT, &now);
        assert(a.item == SET[k]);
    }
    ev(&a, INPUT_BTN_C_SHORT, &now);
    assert(!app_active(&a));

    // No accelerometer: Catch, Maze and Calibrate are not offered, and the
    // games list doesn't open on one of them.
    open_item(&a, ITEM_CATCH, &now);
    app_close(&a);
    has_tilt = false;
    app_open_games(&a, now);
    assert(a.item != ITEM_CATCH);
    for (int i = 0; i < ITEM_COUNT * 2; i++) {
        ev(&a, INPUT_BTN_B_SHORT, &now);
        assert(a.item != ITEM_CATCH && a.item != ITEM_MAZE);
    }
    app_open_settings(&a, now);
    for (int i = 0; i < ITEM_COUNT * 2; i++) {
        ev(&a, INPUT_BTN_B_SHORT, &now);
        assert(a.item != ITEM_SETUP);
    }
    has_tilt = true;
    app_close(&a);

    // Held C goes home from anywhere; so does doing nothing.
    open_item(&a, ITEM_SNAKE, &now);
    ev(&a, INPUT_BTN_C_LONG, &now);
    assert(!app_active(&a));
    app_open_settings(&a, now);
    ev(&a, INPUT_BTN_C_LONG, &now);
    assert(!app_active(&a));
    app_open_games(&a, now);
    run(&a, APP_MENU_IDLE_MS + 100, &now);
    assert(!app_active(&a));

    // In a game: C goes back to its list with it still selected; B is not back.
    open_item(&a, ITEM_DICE, &now);
    press_b(&a, &now);
    assert(a.mode == APP_GAME);
    ev(&a, INPUT_BTN_C_SHORT, &now);
    assert(a.mode == APP_MENU && a.list == 0 && a.item == ITEM_DICE);
    // Held B in a list: the music, silent until a song is chosen. C from the music: the face.
    ev(&a, INPUT_BTN_B_LONG, &now);
    assert(a.mode == APP_GAME && a.item == ITEM_MUSIC && song_on < 0);
    ev(&a, INPUT_BTN_C_SHORT, &now);
    assert(!app_active(&a) && song_on < 0);
}

static void test_settings(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    const app_save_t defaults = APP_SAVE_DEFAULTS;
    app_init(&a, &ENV, &defaults, 14);
    assert(a.save.volume == 2 && a.save.brightness == 1);
    app_open_settings(&a, now);
    ev(&a, INPUT_BTN_B_SHORT, &now);
    assert(a.item == ITEM_VOLUME);

    // A steps the volume round: loud, off, quiet, medium; applied and saved
    // each time, and the menu stays open on it.
    static const int VOL[4] = {3, 0, 1, 2};
    for (int k = 0; k < 4; k++) {
        ev(&a, INPUT_BTN_A_SHORT, &now);
        assert(a.save.volume == VOL[k] && volume_set == VOL[k] && saved.volume == VOL[k]);
        assert(a.mode == APP_MENU && a.item == ITEM_VOLUME);
    }
    assert(saves == 4);
    // Brightness the same: bright, dim, normal.
    ev(&a, INPUT_BTN_B_SHORT, &now);
    assert(a.item == ITEM_BRIGHTNESS);
    static const int BRI[3] = {2, 0, 1};
    for (int k = 0; k < 3; k++) {
        ev(&a, INPUT_BTN_A_SHORT, &now);
        assert(a.save.brightness == BRI[k] && brightness_set == BRI[k] && saved.brightness == BRI[k]);
    }
    assert(app_contrast(0) < app_contrast(1) && app_contrast(1) < app_contrast(2));
    assert(app_contrast(-3) == app_contrast(0) && app_contrast(9) == app_contrast(2));

    // Out-of-range saved values are clamped.
    app_save_t bad = {.volume = 200, .brightness = 9};
    app_init(&a, &ENV, &bad, 15);
    assert(a.save.volume == APP_VOLUME_LEVELS - 1 && a.save.brightness == APP_BRIGHTNESS_LEVELS - 1);
}

static void test_status(void) {
    static app_t a;
    static gfx_t g;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 16);
    // The card draws, with no A word on its hint line; it goes away on its own.
    app_open_settings(&a, now);
    gfx_clear(&g);
    app_render(&a, &g);
    int lit = 0;
    for (int i = 0; i < GFX_FB_BYTES; i++) lit += g.buf[i] != 0;
    assert(lit > 0);
    for (int x = 0; x < 30; x++) {
        for (int y = 57; y < 64; y++) assert(!gfx_get_pixel(&g, x, y));
    }
    // Charging: the battery's corner says so.
    static gfx_t c;
    charging_fake = true;
    gfx_clear(&c);
    app_render(&a, &c);
    bool differs = false;
    for (int x = 90; x < 128; x++) {
        for (int y = 36; y < 48; y++) differs |= gfx_get_pixel(&g, x, y) != gfx_get_pixel(&c, x, y);
    }
    assert(differs);  // "USB" became "CHG"
    charging_fake = false;
    run(&a, APP_MENU_IDLE_MS + 100, &now);
    assert(!app_active(&a));
}

static void test_dice(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 2);
    int seen[7] = {0};
    open_item(&a, ITEM_DICE, &now);
    for (int k = 0; k < 120; k++) {
        ev(&a, k % 2 ? INPUT_SHAKE : INPUT_BTN_A_SHORT, &now);
        assert(a.g.dice.phase == DICE_ROLLING && app_is_animating(&a));
        run(&a, 900, &now);
        assert(a.g.dice.phase == DICE_SHOWN && !app_is_animating(&a));
        assert(a.g.dice.value >= 1 && a.g.dice.value <= 6);
        assert(a.g.dice.shown == a.g.dice.value);  // lands on what it rolled
        seen[a.g.dice.value]++;
    }
    for (int v = 1; v <= 6; v++) assert(seen[v] > 0);
    assert(sounds[SND_ROLL] == 120 && sounds[SND_DING] == 120);
}

static void test_reaction(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 3);
    open_item(&a, ITEM_REACTION, &now);
    assert(a.g.rx.phase == RX_READY);

    // Jumping the gun.
    press_a(&a, &now);
    assert(a.g.rx.phase == RX_WAIT);
    press_a(&a, &now);
    assert(a.g.rx.phase == RX_EARLY && sounds[SND_FAIL] == 1);

    // A fair go: wait for NOW!, press 200 ms later.
    press_a(&a, &now);
    while (a.g.rx.phase == RX_WAIT) app_update(&a, now += FRAME);
    assert(a.g.rx.phase == RX_GO && sounds[SND_GO] == 1);
    now += 200;
    app_on_event(&a, INPUT_BTN_A_PRESS, now);
    assert(a.g.rx.phase == RX_RESULT && a.g.rx.ms == 200);
    assert(a.g.rx.best && saves == 1 && saved.reaction_ms == 200);
    assert(app_take_mood(&a) == EXPR_EXCITED);

    // Slower next time: not a record, the best stays.
    press_a(&a, &now);
    while (a.g.rx.phase == RX_WAIT) app_update(&a, now += FRAME);
    now += 350;
    app_on_event(&a, INPUT_BTN_A_PRESS, now);
    assert(!a.g.rx.best && a.save.reaction_ms == 200 && saves == 1);

    // No press at all: too slow.
    press_a(&a, &now);
    while (a.g.rx.phase == RX_WAIT) app_update(&a, now += FRAME);
    run(&a, 2100, &now);
    assert(a.g.rx.phase == RX_SLOW);
}

static void test_catch(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 4);
    open_item(&a, ITEM_CATCH, &now);

    // Tilt moves the player, even on the title screen.
    tilt_mg = 450;
    run(&a, 500, &now);
    assert(a.g.ctch.x > 100);
    tilt_mg = -450;
    run(&a, 500, &now);
    assert(a.g.ctch.x < 28);
    tilt_mg = 0;
    run(&a, 500, &now);
    assert(a.g.ctch.x > 58 && a.g.ctch.x < 70);

    press_a(&a, &now);
    assert(a.g.ctch.phase == CATCH_PLAY && a.g.ctch.lives == 3);

    // A snowflake dropped on the player scores; a rock costs a life.
    catch_t *c = &a.g.ctch;
    memset(c->obj, 0, sizeof(c->obj));
    c->next_spawn = now + 100000;
    c->obj[0] = (__typeof__(c->obj[0])){true, false, 64, 40 * 16};
    run(&a, 1000, &now);
    assert(c->score == 1 && sounds[SND_POINT] == 1);
    c->obj[0] = (__typeof__(c->obj[0])){true, true, 64, 40 * 16};
    run(&a, 1000, &now);
    assert(c->lives == 2);
    // ...and dodging it is free.
    c->obj[0] = (__typeof__(c->obj[0])){true, true, 10, 40 * 16};
    run(&a, 2000, &now);
    assert(c->lives == 2 && !c->obj[0].on);

    // Out of lives: game over, a record, the face gets excited.
    for (int k = 0; k < 2; k++) {
        c->obj[0] = (__typeof__(c->obj[0])){true, true, 64, 40 * 16};
        run(&a, 1000, &now);
    }
    assert(c->phase == CATCH_OVER && c->best && saved.catch_pts == 1);
    assert(app_take_mood(&a) == EXPR_EXCITED);

    // Playing by tilt alone (no presses) never times out mid-game...
    press_a(&a, &now);
    run(&a, 1000, &now);
    press_a(&a, &now);
    assert(c->phase == CATCH_PLAY);
    c->lives = 1000;  // can't lose while we wait
    run(&a, APP_GAME_IDLE_MS + 5000, &now);
    assert(app_active(&a));
    // ...but the title screen does, even though the player moves on it.
    ev(&a, INPUT_BTN_C_SHORT, &now);
    open_item(&a, ITEM_CATCH, &now);
    run(&a, APP_GAME_IDLE_MS + 1000, &now);
    assert(!app_active(&a));
}

// Every cell of the maze is reachable, by exactly one path: open passages
// number one less than the cells, and a flood fill from the start finds all.
static void check_perfect(const maze_t *m) {
    const int n = m->cols * m->rows;
    int open = 0;
    for (int r = 0; r < m->rows; r++) {
        for (int c = 0; c < m->cols; c++) {
            if (c < m->cols - 1 && !(m->walls[r][c] & 1)) open++;
            if (r < m->rows - 1 && !(m->walls[r][c] & 2)) open++;
        }
    }
    assert(open == n - 1);
    bool seen[MAZE_ROWS * MAZE_COLS] = {false};
    int queue[MAZE_ROWS * MAZE_COLS], head = 0, tail = 0, found = 1;
    queue[tail++] = 0;
    seen[0] = true;
    while (head < tail) {
        const int cur = queue[head++], c = cur % m->cols, r = cur / m->cols;
        const int nb[4] = {c < m->cols - 1 && !(m->walls[r][c] & 1) ? cur + 1 : -1,
                           c > 0 && !(m->walls[r][c - 1] & 1) ? cur - 1 : -1,
                           r < m->rows - 1 && !(m->walls[r][c] & 2) ? cur + m->cols : -1,
                           r > 0 && !(m->walls[r - 1][c] & 2) ? cur - m->cols : -1};
        for (int k = 0; k < 4; k++) {
            if (nb[k] < 0 || seen[nb[k]]) continue;
            seen[nb[k]] = true;
            queue[tail++] = nb[k];
            found++;
        }
    }
    assert(found == n);
    assert(m->goal > 0 && m->goal < n);
}

static void solve(app_t *a, uint32_t *now) {
    maze_t *m = &a->g.maze;
    m->x = (m->ox + (m->goal % m->cols) * m->cell + m->cell / 2) * 16;
    m->y = (m->oy + (m->goal / m->cols) * m->cell + m->cell / 2) * 16;
    m->vx = m->vy = 0;
    run(a, FRAME, now);
}

static void test_maze(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 5);
    open_item(&a, ITEM_MAZE, &now);
    maze_t *m = &a.g.maze;
    assert(m->phase == MAZE_START);

    // However it is held when the run starts counts as level: the ball stays put.
    tilt_xy[0] = 300;
    tilt_xy[1] = -200;
    press_a(&a, &now);
    assert(m->phase == MAZE_PLAY && m->cell == 16 && m->cols == 7 && m->rows == 3);
    check_perfect(m);
    const int x0 = m->x, y0 = m->y;
    run(&a, 1000, &now);
    assert(m->x == x0 && m->y == y0);

    // Tilted toward an open side, it rolls that way.
    const bool right_open = !(m->walls[0][0] & 1);
    tilt_xy[0] = 300 + (right_open ? 400 : 0);
    tilt_xy[1] = -200 + (right_open ? 0 : 400);
    run(&a, 400, &now);
    assert(right_open ? m->x > x0 + 16 : m->y > y0 + 16);
    // Tilted hard into the outer walls, it stays inside the maze.
    tilt_xy[0] = 300 - 1000;
    tilt_xy[1] = -200 - 1000;
    run(&a, 3000, &now);
    assert(m->x / 16 >= m->ox + 3 && m->y / 16 >= m->oy + 3);
    assert(m->x / 16 <= m->ox + m->cols * m->cell - 3 && m->y / 16 <= m->oy + m->rows * m->cell - 3);
    tilt_xy[0] = 300;
    tilt_xy[1] = -200;

    // Reaching the ring: a point, more time, and a bigger maze with smaller cells.
    const int left = m->left_ms;
    solve(&a, &now);
    assert(m->phase == MAZE_SOLVED && m->score == 1 && m->left_ms > left && sounds[SND_POINT] == 1);
    run(&a, 800, &now);
    assert(m->phase == MAZE_PLAY && m->level == 1 && m->cell == 12);
    check_perfect(m);
    for (int k = 2; k <= 5; k++) {
        solve(&a, &now);
        run(&a, 800, &now);
        assert(m->level == k && m->cell == (k == 2 ? 10 : 8));
        check_perfect(m);
        assert(m->cell != 8 || (m->cols == MAZE_COLS && m->rows == MAZE_ROWS));
    }

    // Played by tilt alone, it never times out mid-run...
    m->left_ms = APP_GAME_IDLE_MS + 10000;
    run(&a, APP_GAME_IDLE_MS + 5000, &now);
    assert(app_active(&a) && m->phase == MAZE_PLAY);
    // ...and when the time runs out: game over, and the best is saved.
    run(&a, 6000, &now);
    assert(m->phase == MAZE_OVER && m->best && saved.maze_pts == 5);
    assert(app_take_mood(&a) == EXPR_EXCITED);

    // Without an accelerometer it isn't in the menu.
    reset_fakes();
    has_tilt = false;
    app_init(&a, &ENV, NULL, 5);
    app_open_games(&a, now);
    for (int k = 0; k < 20; k++) {
        assert(a.item != ITEM_MAZE);
        ev(&a, INPUT_BTN_B_SHORT, &now);
    }
}

static void test_flappy(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 5);
    open_item(&a, ITEM_FLAPPY, &now);
    assert(a.g.fl.phase == FLAPPY_START);
    run(&a, 3000, &now);
    assert(a.g.fl.phase == FLAPPY_START);  // waits for the first flap

    // No flapping: falls and crashes within a couple of seconds.
    press_a(&a, &now);
    assert(a.g.fl.phase == FLAPPY_PLAY);
    for (int k = 0; k < 100 && a.g.fl.phase == FLAPPY_PLAY; k++) app_update(&a, now += FRAME);
    assert(a.g.fl.phase == FLAPPY_OVER && sounds[SND_FAIL] == 1);

    // Right after the crash a press is ignored, so a frantic flap doesn't skip
    // past the score.
    press_a(&a, &now);
    assert(a.g.fl.phase == FLAPPY_OVER);
    run(&a, 900, &now);
    press_a(&a, &now);
    assert(a.g.fl.phase == FLAPPY_START);

    // Flapping in rhythm with every gap lined up: it scores.
    press_a(&a, &now);
    flappy_t *f = &a.g.fl;
    for (int i = 0; i < FLAPPY_PIPES; i++) f->pipe[i].gap = 30;
    for (int k = 0; k < 400 && f->phase == FLAPPY_PLAY; k++) {
        for (int i = 0; i < FLAPPY_PIPES; i++) f->pipe[i].gap = 30;  // recycled pipes too
        if (f->y / 16 > 32 && f->vy > 0) app_on_event(&a, INPUT_BTN_A_PRESS, now);
        app_update(&a, now += FRAME);
    }
    printf("  flappy score after 400 frames: %d\n", f->score);
    assert(f->phase == FLAPPY_PLAY && f->score >= 5);
}

static void test_snake(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 9);
    open_item(&a, ITEM_SNAKE, &now);
    snake_t *s = &a.g.snake;
    assert(s->phase == SNAKE_START);
    // C on the title goes back to the menu; B does nothing there.
    press_b(&a, &now);
    assert(a.mode == APP_GAME && s->phase == SNAKE_START);
    ev(&a, INPUT_BTN_C_SHORT, &now);
    assert(a.mode == APP_MENU);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    press_a(&a, &now);
    assert(s->phase == SNAKE_PLAY && s->len == 4 && s->dir == 0);

    // Food right ahead: it eats and grows.
    s->food[0] = s->body[s->head][0] + 2;
    s->food[1] = s->body[s->head][1];
    run(&a, 350, &now);  // two steps (the berry it places next could be anywhere)
    assert(s->score == 1 && s->len == 5 && sounds[SND_POINT] == 1);

    // B turns right (down) and stays in the game.
    press_b(&a, &now);
    assert(a.mode == APP_GAME);
    run(&a, 250, &now);
    assert(s->dir == 1);
    // A: one left turn, heading along the row again, one below; still alive.
    press_a(&a, &now);
    run(&a, 400, &now);
    assert(s->dir == 0 || s->dir == 2);
    assert(s->phase == SNAKE_PLAY);

    // Run it into the wall: game over, a record, back to the title after.
    for (int k = 0; k < 400 && s->phase == SNAKE_PLAY; k++) app_update(&a, now += FRAME);
    assert(s->phase == SNAKE_OVER && s->best && saved.snake_pts == 1);
    press_a(&a, &now);
    assert(s->phase == SNAKE_OVER);  // too soon after the crash
    run(&a, 900, &now);
    press_a(&a, &now);
    assert(s->phase == SNAKE_PLAY && s->score == 0);
}

static void test_setup(void) {
    // Board mounted with its X axis up and Z out of the screen.
    int8_t map[3];
    const int flat[3] = {30, -20, 1010}, upright[3] = {990, 40, -60};
    assert(orient_solve(flat, upright, map));
    int out[3];
    orient_apply(map, upright, out);
    assert(out[1] > 900 && abs(out[0]) < 100 && abs(out[2]) < 100);  // up is up
    orient_apply(map, flat, out);
    assert(out[2] > 900);  // out of the screen
    // right = up x out: board X x board Z = -board Y.
    assert(map[0] == -2 && map[1] == 1 && map[2] == 3);
    // Same axis twice, or no clear axis: refused.
    assert(!orient_solve(flat, flat, map));
    const int vague[3] = {500, 500, 500};
    assert(!orient_solve(vague, upright, map));
    // Uncalibrated: passes through.
    const int8_t none[3] = {0, 0, 0};
    orient_apply(none, upright, out);
    assert(out[0] == 990 && out[1] == 40 && out[2] == -60);

    // The screen: two steps, then it applies and saves the result.
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 11);
    open_item(&a, ITEM_SETUP, &now);
    memcpy(raw_mg, flat, sizeof(raw_mg));
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(a.g.setup.phase == SETUP_UPRIGHT);
    memcpy(raw_mg, upright, sizeof(raw_mg));
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(a.g.setup.phase == SETUP_DONE && orients == 1 && applied[1] == 1);
    assert(saved.orient[0] == -2 && a.save.orient[2] == 3);
    // Held wrong the second time: try again, nothing applied.
    ev(&a, INPUT_BTN_A_SHORT, &now);
    memcpy(raw_mg, flat, sizeof(raw_mg));
    ev(&a, INPUT_BTN_A_SHORT, &now);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(a.g.setup.phase == SETUP_FAIL && orients == 1);
    // Not lying flat at the first step: straight to try again.
    ev(&a, INPUT_BTN_A_SHORT, &now);
    const int tilted[3] = {500, 500, 500};
    memcpy(raw_mg, tilted, sizeof(raw_mg));
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(a.g.setup.phase == SETUP_FAIL);
}

static int sent_of(uint8_t type) {
    int n = 0;
    for (int i = 0; i < nsent; i++) n += sent[i].type == type;
    return n;
}

static const bl_msg_t *last_sent(uint8_t type) {
    for (int i = nsent - 1; i >= 0; i--)
        if (sent[i].type == type) return &sent[i];
    return NULL;
}

static void test_toss(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 3);
    open_item(&a, ITEM_BIT, &now);
    assert(a.g.toss.phase == TOSS_AIM && a.g.toss.item == BL_ITEM_HEART);
    assert(app_wants_link(&a));

    // B picks the next thing, round the three; A throws it.
    ev(&a, INPUT_BTN_B_SHORT, &now);
    assert(a.g.toss.item == BL_ITEM_STAR);
    press_a(&a, &now);
    assert(a.g.toss.phase == TOSS_FLYING);
    const bl_msg_t *m = last_sent(BL_THROW);
    assert(m && m->d == BL_ITEM_STAR && sent_reliable[nsent - 1]);
    // Taken by BIT's radio, then BIT's word on it.
    app_on_link_done(&a, m, true, now);
    run(&a, 500, &now);
    assert(a.g.toss.phase == TOSS_WAIT);
    const bl_msg_t caught = {.type = BL_CAUGHT, .d = BL_ITEM_STAR, .e = BL_CAUGHT_WOW};
    app_on_link(&a, &caught, now);
    assert(a.g.toss.phase == TOSS_RESULT && a.g.toss.result == BL_CAUGHT_WOW);
    assert(app_take_mood(&a) == EXPR_EXCITED);
    run(&a, 3000, &now);
    assert(a.g.toss.phase == TOSS_AIM);

    // A shake throws too; never delivered: missed.
    ev(&a, INPUT_SHAKE, &now);
    assert(a.g.toss.phase == TOSS_FLYING);
    app_on_link_done(&a, last_sent(BL_THROW), false, now);
    assert(a.g.toss.phase == TOSS_RESULT && a.g.toss.result == TOSS_MISSED);
    run(&a, 3000, &now);

    // Delivered, but no word comes: it got there, and the screen says so.
    press_a(&a, &now);
    app_on_link_done(&a, last_sent(BL_THROW), true, now);
    run(&a, 6000, &now);
    assert(a.g.toss.phase == TOSS_RESULT && a.g.toss.result == 0);

    // No BIT found yet: nothing is sent.
    run(&a, 3000, &now);
    link_fake = APP_LINK_SEARCHING;
    const int before = nsent;
    press_a(&a, &now);
    assert(nsent == before && a.g.toss.result == TOSS_NO_BIT);
    app_close(&a);
    assert(!app_wants_link(&a));
}

static void test_pong(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 4);
    open_item(&a, ITEM_PONG, &now);
    assert(a.g.pong.phase == PK_TITLE);

    // A asks; BIT says no, and the title comes back.
    press_a(&a, &now);
    assert(a.g.pong.phase == PK_ASKING && sent_of(BL_INVITE) == 1);
    const bl_msg_t no = {.type = BL_ANSWER, .d = 0, .e = BL_NO_DECLINED};
    app_on_link(&a, &no, now);
    assert(a.g.pong.phase == PK_NO && a.g.pong.why == BL_NO_DECLINED);
    run(&a, 3000, &now);
    assert(a.g.pong.phase == PK_TITLE);

    // Asked again, and nobody answers: the question is withdrawn.
    press_a(&a, &now);
    run(&a, 16000, &now);
    assert(a.g.pong.phase == PK_NO && a.g.pong.why == BL_NO_TIMEOUT);
    assert(sent_of(BL_QUIT) == 1);
    run(&a, 3000, &now);

    // Yes: YETI serves, the ball goes up and over to BIT.
    press_a(&a, &now);
    const bl_msg_t yes = {.type = BL_ANSWER, .d = 1};
    app_on_link(&a, &yes, now);
    assert(a.g.pong.phase == PK_PLAY && a.g.pong.pong.phase == PONG_SERVE);
    for (int k = 0; k < 150 && sent_of(BL_BALL) == 0; k++) {
        heard_fake = now;
        run(&a, FRAME, &now);
    }
    assert(sent_of(BL_BALL) == 1 && a.g.pong.pong.phase == PONG_AWAY);
    assert(sent_of(BL_ALIVE) > 0);
    // BIT sends it back; the tilt puts the paddle under it.
    const bl_msg_t back = {.type = BL_BALL, .a = 64 * 256, .b = 0, .c = -60 * 256};
    app_on_link(&a, &back, now);
    assert(a.g.pong.pong.phase == PONG_MINE);
    tilt_mg = 0;
    bool returned = false;
    for (int k = 0; k < 100 && !returned; k++) {
        heard_fake = now;
        run(&a, FRAME, &now);
        returned = sent_of(BL_BALL) == 2;
    }
    assert(returned);
    // Every screen of it draws.
    static gfx_t g;
    gfx_clear(&g);
    app_render(&a, &g);
    // C leaves the match, and says so.
    ev(&a, INPUT_BTN_C_SHORT, &now);
    assert(a.g.pong.phase == PK_OVER && a.g.pong.end == PK_I_QUIT);
    assert(last_sent(BL_QUIT) != NULL && sent_of(BL_QUIT) == 2);

    // A match closed from outside - the night, C held - says goodbye too.
    press_a(&a, &now);
    app_on_link(&a, &yes, now);
    assert(a.g.pong.phase == PK_PLAY);
    app_close(&a);
    assert(sent_of(BL_QUIT) == 3);

    // BIT wins it.
    open_item(&a, ITEM_PONG, &now);
    press_a(&a, &now);
    app_on_link(&a, &yes, now);
    a.g.pong.pong.them = PONG_WIN - 1;
    heard_fake = now;
    for (int k = 0; k < 400 && a.g.pong.phase == PK_PLAY; k++) {
        tilt_mg = 2000;  // the paddle hard over: it misses
        heard_fake = now;
        run(&a, FRAME, &now);
        if (a.g.pong.pong.phase == PONG_AWAY) {
            const bl_msg_t in = {.type = BL_BALL, .a = 120 * 256, .b = 0, .c = -70 * 256};
            app_on_link(&a, &in, now);
        }
    }
    assert(a.g.pong.phase == PK_OVER && a.g.pong.end == PONG_END_LOST);
    assert(app_take_mood(&a) == EXPR_SAD);
    app_close(&a);
}

static void test_update(void) {
    static app_t a;
    static gfx_t g;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 5);
    // Off until chosen: nothing starts the WiFi but opening UPDATE.
    app_open_settings(&a, now);
    assert(upd_begins == 0);
    app_close(&a);
    open_item(&a, ITEM_UPDATE, &now);
    assert(upd_begins == 1 && a.g.upd.started);
    gfx_clear(&g);
    app_render(&a, &g);
    // Waiting is not idle: the list's timeout does not take the WiFi down.
    run(&a, APP_GAME_IDLE_MS + 5000, &now);
    assert(a.mode == APP_GAME && a.item == ITEM_UPDATE && upd_ends == 0);
    // C leaves, and the WiFi goes with it.
    ev(&a, INPUT_BTN_C_SHORT, &now);
    assert(upd_ends == 1 && a.mode == APP_MENU);

    // Ten minutes and nobody came: it closes itself.
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(upd_begins == 2);
    run(&a, 10 * 60 * 1000 + 1000, &now);
    assert(!app_active(&a) && upd_ends == 2);

    // Coming in: the bar; failed: A tries again.
    open_item(&a, ITEM_UPDATE, &now);
    upd_fake.phase = APP_UPD_RECEIVING;
    upd_fake.pct = 40;
    gfx_clear(&g);
    app_render(&a, &g);
    upd_fake.phase = APP_UPD_FAILED;
    upd_fake.why = "WRONG FIRMWARE";
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(upd_fake.phase == APP_UPD_WAITING && upd_begins == 4);
    // Closed from outside - C held, the night - it goes too.
    app_close(&a);
    assert(upd_ends == 4);

    // A build without it: not in the list.
    static app_env_t bare;
    bare = ENV;
    bare.update_begin = NULL;
    app_init(&a, &bare, NULL, 5);
    app_open_settings(&a, now);
    for (int k = 0; k < 8; k++) {
        ev(&a, INPUT_BTN_B_SHORT, &now);
        assert(a.item != ITEM_UPDATE);
    }
    app_close(&a);
}

static void test_render_all(void) {
    // Every screen draws something and nothing crashes (ASan watches).
    static app_t a;
    static gfx_t g;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 12);
    for (int it = 0; it < ITEM_COUNT; it++) {
        if (it == ITEM_STATUS || it == ITEM_VOLUME || it == ITEM_BRIGHTNESS) continue;
        open_item(&a, (app_item_t)it, &now);
        for (int k = 0; k < 4; k++) {
            press_a(&a, &now);
            run(&a, 700, &now);
            gfx_clear(&g);
            app_render(&a, &g);
            int lit = 0;
            for (int i = 0; i < GFX_FB_BYTES; i++) lit += g.buf[i] != 0;
            assert(lit > 0);
        }
        app_close(&a);
    }
    // And every item of both lists, at every setting.
    for (int l = 0; l < 2; l++) {
        if (l) {
            app_open_settings(&a, now);
        } else {
            app_open_games(&a, now);
        }
        for (int k = 0; k < 8; k++) {
            for (int v = 0; v < APP_VOLUME_LEVELS; v++) {
                a.save.volume = (uint8_t)v;
                a.save.brightness = (uint8_t)(v % APP_BRIGHTNESS_LEVELS);
                gfx_clear(&g);
                app_render(&a, &g);
            }
            ev(&a, INPUT_BTN_B_SHORT, &now);
            run(&a, 300, &now);
        }
        ev(&a, INPUT_BTN_C_SHORT, &now);
    }
}

static void test_rtttl(void) {
    note_t n[64];
    // a quarter at 120 bpm = 500 ms; dotted = 750; rests stay rests; A7 = 3520.
    const int k = rtttl_parse("T:d=4,o=7,b=120:a,8p,a.,2c#6,16b#", n, 64, 0);
    assert(k == 5);
    assert(n[0].hz == 3520 && n[0].ms == 500);
    assert(n[1].hz == 0 && n[1].ms == 250);
    assert(n[2].hz == 3520 && n[2].ms == 750);
    assert(n[3].hz == 1108 && n[3].ms == 1000);  // C#6
    assert(n[4].hz == 4186 && n[4].ms == 125);   // b#7 = C8
    assert(n[5].hz == 0 && n[5].ms == 0);
    // Gaps split notes, never rests; a tiny buffer stays terminated.
    assert(rtttl_parse("T:d=4,o=7,b=120:a,p,a", n, 64, 20) == 5);
    assert(n[0].ms == 480 && n[1].hz == 0 && n[1].ms == 20 && n[2].hz == 0 && n[2].ms == 500);
    assert(n[3].ms == 480 && n[4].ms == 20);
    assert(rtttl_parse("T:d=4,o=7,b=120:a,a,a,a,a,a", n, 3, 0) == 2 && n[2].ms == 0);
    assert(rtttl_parse("garbage", n, 64, 0) == 0 && n[0].ms == 0);
    // Some collections write a sharp as "_".
    assert(rtttl_parse("T:d=4,o=7,b=120:c_6,c#6", n, 64, 0) == 2 && n[0].hz == 1108 && n[1].hz == 1108);
    // Every song parses, fits the player's buffer, and sits where the
    // buzzer is loud (no note below 1 kHz or above 4.5 kHz).
    static note_t big[320];
    assert(songs_count() == 11);
    for (int s = 0; s < songs_count(); s++) {
        const int m = rtttl_parse(song_rtttl(s), big, 320, 18);
        uint32_t ms = 0;
        for (int i = 0; i < m; i++) {
            ms += big[i].ms;
            assert(!big[i].hz || (big[i].hz >= 1000 && big[i].hz <= 4500));
        }
        printf("  %-15s %3d steps %5.1f s\n", song_name(s), m, ms / 1000.0);
        assert(m > 20 && m < 318 && ms > 5000 && ms < 40000);
    }
}

static void test_story(void) {
    static story_t s;
    uint32_t now = 1000;
    story_init(&s, 5, now);
    int glitches = 0, songs = 0, hangs = 0, last_song = -1;
    uint32_t last_event = 0;
    bool spaced = true;
    const char *first = NULL;
    // Eight hours, happy and idle: glitches and songs, never two close together.
    for (uint32_t t = 0; t < 8u * 3600u * 1000u; t += 200) {
        const story_event_t e = story_update(&s, now += 200, true, true);
        if (e == STORY_NONE) continue;
        if (last_event && now - last_event < STORY_GAP_MS) spaced = false;
        last_event = now;
        if (e == STORY_GLITCH) {
            glitches++;
            const char *line = story_log(&s);
            if (!first) first = line;
        }
        if (e == STORY_SONG) {
            assert(story_song(&s) != last_song && story_song(&s) < songs_count());
            last_song = story_song(&s);
            songs++;
        }
        hangs += e == STORY_HANG;
    }
    printf("  8 h: %d glitches, %d songs, %d hangs\n", glitches, songs, hangs);
    assert(spaced && glitches >= 8 && glitches <= 35 && songs >= 5 && songs <= 20);
    assert(hangs >= 6 && hangs <= 25);
    assert(strcmp(first, "YETI-7 ONLINE") == 0);  // the story starts at the start

    // Busy (menu open, night): nothing starts; it waits for idle.
    story_init(&s, 7, now);
    for (int k = 0; k < 2 * 3600 * 5; k++) assert(story_update(&s, now += 200, false, true) == STORY_NONE);
    assert(story_update(&s, now += 200, true, true) != STORY_NONE);
    // Over a few days it sings every song the keychain knows (story.c keeps
    // its own count of them).
    story_init(&s, 9, now);
    bool sung[64] = {false};
    for (uint32_t t = 0; t < 96u * 3600u * 1000u; t += 1000) {
        if (story_update(&s, now += 1000, true, true) == STORY_SONG) sung[story_song(&s)] = true;
    }
    for (int k = 0; k < songs_count(); k++) assert(sung[k]);
    // Sound off: no songs, ever.
    story_init(&s, 8, now);
    for (int k = 0; k < 8 * 3600 * 5; k++) assert(story_update(&s, now += 200, true, false) != STORY_SONG);
}

static void test_music(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 13);
    // Held B: the chooser on the first song, nothing playing yet.
    app_open_music(&a, now);
    assert(a.mode == APP_GAME && a.item == ITEM_MUSIC);
    assert(song_on == -1 && !a.g.music.playing && a.g.music.song == 0);
    // B steps through the songs, silently; A plays the one showing.
    ev(&a, INPUT_BTN_B_SHORT, &now);
    assert(song_on == -1 && a.g.music.song == 1 && sounds[SND_MOVE] == 1);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(song_on == 1 && a.g.music.playing && app_is_animating(&a));
    // Plays on without input (no idle timeout mid-song), the animation fed its notes.
    run(&a, APP_GAME_IDLE_MS + 1000, &now);
    assert(app_active(&a) && song_on == 1 && a.g.music.viz.notes_seen > 20);
    // B plays the next song at once, with a different animation.
    const int viz = a.g.music.viz.kind;
    ev(&a, INPUT_BTN_B_SHORT, &now);
    assert(song_on == 2 && a.g.music.song == 2 && a.g.music.playing && (int)a.g.music.viz.kind != viz);
    // A stops it: back to the chooser, still on that song. C as well, not leaving.
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(song_on == -1 && !a.g.music.playing && a.mode == APP_GAME);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    ev(&a, INPUT_BTN_C_SHORT, &now);
    assert(song_on == -1 && !a.g.music.playing && a.mode == APP_GAME);
    // Finished by itself: back to the chooser.
    ev(&a, INPUT_BTN_A_SHORT, &now);
    song_on = -1;
    run(&a, 100, &now);
    assert(!a.g.music.playing && a.mode == APP_GAME);
    // C in the chooser: the face. Next time it opens on the song played last.
    ev(&a, INPUT_BTN_C_SHORT, &now);
    assert(!app_active(&a));
    app_open_music(&a, now);
    assert(song_on == -1 && a.g.music.song == 2);
    // Never the same animation twice in a row, and every one comes up.
    int seen[VIZ_COUNT] = {0}, prev = -1;
    ev(&a, INPUT_BTN_A_SHORT, &now);
    for (int k = 0; k < 300; k++) {
        const int v = a.g.music.viz.kind;
        assert(v != prev);
        seen[v]++;
        prev = v;
        ev(&a, INPUT_BTN_B_SHORT, &now);
    }
    for (int v = 0; v < VIZ_COUNT; v++) assert(seen[v] > 10);
    ev(&a, INPUT_BTN_C_LONG, &now);
    assert(!app_active(&a) && song_on == -1);
    // Sound off: a fail beep, SOUND OFF on the screen, nothing plays.
    muted_fake = true;
    app_open_music(&a, now);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(!a.g.music.playing && sounds[SND_FAIL] == 1 && a.g.music.msg_until);
    // B goes round all the songs and back to where it started.
    const int at = a.g.music.song;
    for (int k = 0; k < songs_count(); k++) ev(&a, INPUT_BTN_B_SHORT, &now);
    assert(a.g.music.song == at && a.mode == APP_GAME);
}

// Every game and every song has a picture of its own.
static void test_art(void) {
    static gfx_t g, fallback;
    gfx_clear(&fallback);
    art_song(&fallback, "NO SUCH SONG");
    for (int s = 0; s < songs_count(); s++) {
        gfx_clear(&g);
        art_song(&g, song_name(s));
        int lit = 0;
        for (int i = 0; i < GFX_FB_BYTES; i++) lit += g.buf[i] != 0;
        assert(lit > 20 && memcmp(g.buf, fallback.buf, GFX_FB_BYTES) != 0);
        for (int x = 0; x < GFX_W; x++)  // the name's rows below stay clear
            for (int y = 40; y < GFX_H; y++) assert(!gfx_get_pixel(&g, x, y));
    }
    for (int it = ITEM_DICE; it <= ITEM_VOICE; it++) {
        gfx_clear(&g);
        art_game(&g, (app_item_t)it);
        int lit = 0;
        for (int i = 0; i < GFX_FB_BYTES; i++) lit += g.buf[i] != 0;
        assert(lit > 20);
        for (int x = 0; x < GFX_W; x++) {
            for (int y = 40; y < GFX_H; y++) assert(!gfx_get_pixel(&g, x, y));
            if (x >= 121) for (int y = 0; y < GFX_H; y++) assert(!gfx_get_pixel(&g, x, y));  // the list's dots
        }
    }
}

// Every animation with every song: notes in, frames out, nothing outside
// the screen's memory (ASan), something on screen most of the time.
static void test_viz(void) {
    static viz_track_t tr;
    static viz_t v;
    static gfx_t g;
    rng_t rng;
    rng_seed(&rng, 5);
    for (int s = 0; s < songs_count(); s++) {
        assert(viz_track_load(&tr, s));
        int lo = 255, hi = 0;
        for (int i = 0; i < tr.n; i++) {
            if (!tr.notes[i].hz) continue;
            lo = tr.pitch[i] < lo ? tr.pitch[i] : lo;
            hi = tr.pitch[i] > hi ? tr.pitch[i] : hi;
        }
        assert(lo == 0 && hi == 255);  // the song's range, end to end
        for (int k = 0; k < VIZ_COUNT; k++) {
            viz_start(&v, (viz_kind_t)k, &rng);
            int next = 0, blank = 0, frames = 0;
            uint32_t next_at = 0;
            for (uint32_t t = 0; t < 20000; t += 33) {
                while (next < tr.n && next_at <= t) {
                    if (tr.notes[next].hz) viz_note(&v, &tr, next, &rng);
                    next_at += tr.notes[next++].ms;
                }
                viz_step(&v, 33, &rng);
                gfx_clear(&g);
                viz_draw(&v, &tr, t, &g);
                int lit = 0;
                for (int i = 0; i < GFX_FB_BYTES; i++) lit += g.buf[i] != 0;
                blank += lit == 0;
                frames++;
            }
            if (blank * 20 >= frames) printf("  song %d viz %d: %d of %d frames blank\n", s, k, blank, frames);
            assert(blank * 20 < frames);
        }
    }
}

static void test_egg(void) {
    static app_t a;
    static gfx_t g;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 21);
    // Five presses and a shake: nothing. Six, then a shake: 67.
    for (int k = 0; k < 5; k++) app_egg_watch(&a, INPUT_BTN_A_SHORT, now += 300);
    assert(!app_egg_watch(&a, INPUT_SHAKE, now += 300) && !app_active(&a));
    for (int k = 0; k < 6; k++) app_egg_watch(&a, INPUT_BTN_A_SHORT, now += 300);
    assert(app_egg_watch(&a, INPUT_SHAKE, now += 2000) && a.mode == APP_GAME && a.item == ITEM_EGG);
    // It plays to the end by itself, every cue once, a tick per swing, then an excited face.
    int frames = 0, dark = 0;
    while (app_active(&a)) {
        ev(&a, INPUT_BTN_A_SHORT, &now);  // ignored meanwhile
        gfx_clear(&g);
        app_render(&a, &g);
        int lit = 0;
        for (int i = 0; i < GFX_FB_BYTES; i++) lit += g.buf[i] != 0;
        dark += lit == 0;
        frames++;
        assert(frames < EGG_MS / FRAME + 10);
    }
    assert(frames > EGG_MS / FRAME - 10 && dark < 3);
    assert(sounds[SND_EGG_RISE] == 1 && sounds[SND_EGG_SIX] == 1 && sounds[SND_EGG_SEVEN] == 1);
    assert(sounds[SND_EGG_BOOM] == 1 && sounds[SND_RECORD] == 1);
    assert(sounds[SND_EGG_TICK] >= 8 && sounds[SND_EGG_TICK] <= 10);
    assert(app_take_mood(&a) == EXPR_EXCITED);
    // Too slow: presses far apart, or the shake too late.
    for (int k = 0; k < 6; k++) app_egg_watch(&a, INPUT_BTN_A_SHORT, now += 1500);
    assert(!app_egg_watch(&a, INPUT_SHAKE, now += 300));
    for (int k = 0; k < 6; k++) app_egg_watch(&a, INPUT_BTN_A_SHORT, now += 300);
    assert(!app_egg_watch(&a, INPUT_SHAKE, now += 3500));
    // Another button in between starts it over.
    for (int k = 0; k < 6; k++) app_egg_watch(&a, INPUT_BTN_A_SHORT, now += 300);
    app_egg_watch(&a, INPUT_BTN_B_SHORT, now += 100);
    assert(!app_egg_watch(&a, INPUT_SHAKE, now += 300));
    // C ends it early.
    open_item(&a, ITEM_EGG, &now);
    run(&a, 1000, &now);
    ev(&a, INPUT_BTN_C_SHORT, &now);
    assert(!app_active(&a));
}

static void test_voice(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 17);
    open_item(&a, ITEM_VOICE, &now);
    voice_t *v = &a.g.voice;
    assert(v->phase == VOICE_START && !v->no_mic && app_wants_mic(&a));
    // Shouting on the title doesn't start it; A does.
    loudness_fake = 80;
    run(&a, 500, &now);
    assert(v->phase == VOICE_START && v->y == 0);
    loudness_fake = 0;
    press_a(&a, &now);
    assert(v->phase == VOICE_PLAY);

    // Quiet: stays on the ground. A shout: it jumps, higher the louder.
    run(&a, 300, &now);
    assert(v->y == 0);
    int peak[2] = {0, 0};
    for (int k = 0; k < 2; k++) {
        for (int i = 0; i < VOICE_ROCKS; i++) v->rock[i].x = 400 * 16;  // out of the way
        loudness_fake = k ? 100 : 25;
        app_update(&a, now += FRAME);
        loudness_fake = 0;
        for (int f = 0; f < 60; f++) {
            for (int i = 0; i < VOICE_ROCKS; i++) v->rock[i].x = 400 * 16;
            app_update(&a, now += FRAME);
            if (v->y > peak[k]) peak[k] = v->y;
        }
        assert(v->y == 0);  // landed
    }
    printf("  jump peaks: %d (soft), %d (loud) / 16 px\n", peak[0], peak[1]);
    assert(peak[0] > 0 && peak[1] > peak[0] * 2);
    // Humming on holds it up longer than a single shout.
    int air_shout = 0, air_hum = 0;
    for (int k = 0; k < 2; k++) {
        loudness_fake = 100;
        for (int f = 0; f < 90; f++) {
            for (int i = 0; i < VOICE_ROCKS; i++) v->rock[i].x = 400 * 16;
            app_update(&a, now += FRAME);
            if (f == 0 && !k) loudness_fake = 0;
            if (v->y > 0) *(k ? &air_hum : &air_shout) += 1;
            if (k && f > 0 && v->y == 0) loudness_fake = 0;  // landed: stop, or it jumps again
        }
        loudness_fake = 0;
        run(&a, 1500, &now);
    }
    assert(air_hum > air_shout);

    // Rocks: jumped over, they score; walked into, game over and a record.
    for (int i = 0; i < VOICE_ROCKS; i++) v->rock[i].x = 400 * 16;
    v->rock[0].x = 40 * 16;
    v->rock[0].h = 8;
    loudness_fake = 70;
    for (int f = 0; f < 60 && !v->rock[0].passed; f++) {
        app_update(&a, now += FRAME);
        if (v->y > 0) loudness_fake = 0;
    }
    assert(v->rock[0].passed && v->score >= 1 && v->phase == VOICE_PLAY);
    loudness_fake = 0;
    run(&a, 1500, &now);
    v->rock[1].x = 40 * 16;
    v->rock[1].h = 10;
    for (int f = 0; f < 200 && v->phase == VOICE_PLAY; f++) app_update(&a, now += FRAME);
    assert(v->phase == VOICE_OVER && v->best && saved.voice_pts == v->score);
    assert(app_take_mood(&a) == EXPR_EXCITED);
    // Playing by voice alone never times out mid-game.
    run(&a, 900, &now);
    press_a(&a, &now);
    press_a(&a, &now);
    assert(v->phase == VOICE_PLAY);
    for (int f = 0; f < (APP_GAME_IDLE_MS + 2000) / FRAME; f++) {
        for (int i = 0; i < VOICE_ROCKS; i++) v->rock[i].x = 400 * 16;
        app_update(&a, now += FRAME);
    }
    assert(app_active(&a));
    ev(&a, INPUT_BTN_C_SHORT, &now);
    assert(!app_wants_mic(&a));

    // No mic: says so, and A jumps.
    loudness_fake = -1;
    open_item(&a, ITEM_VOICE, &now);
    assert(v->no_mic);
    press_a(&a, &now);
    for (int i = 0; i < VOICE_ROCKS; i++) v->rock[i].x = 400 * 16;
    press_a(&a, &now);
    run(&a, 100, &now);
    assert(v->y > 0);
}

int main(void) {
#define RUN(t) (printf("%s\n", #t), t())
    RUN(test_menu);
    RUN(test_settings);
    RUN(test_status);
    RUN(test_dice);
    RUN(test_reaction);
    RUN(test_catch);
    RUN(test_maze);
    RUN(test_flappy);
    RUN(test_snake);
    RUN(test_voice);
    RUN(test_setup);
    RUN(test_toss);
    RUN(test_pong);
    RUN(test_update);
    RUN(test_render_all);
    RUN(test_rtttl);
    RUN(test_story);
    RUN(test_music);
    RUN(test_viz);
    RUN(test_art);
    RUN(test_egg);
    printf("all app tests passed\n");
    return 0;
}
