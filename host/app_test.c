// Host tests for the menu and the games, with a fake environment that
// records sounds and saves and lets a test set the tilt.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app.h"
#include "config.h"
#include "face/face.h"
#include "input/orient.h"
#include "pet/pet.h"
#include "pet/story.h"
#include "sound/rtttl.h"

#define FRAME 33

static int sounds[SND_COUNT];
static int saves;
static app_save_t saved;
static bool has_tilt;
static int tilt_mg;
static int raw_mg[3];
static int8_t applied[3];
static int orients;
static int song_on = -1;  // the song "playing", -1 = none
static bool muted_fake;

static void fake_play(app_sound_t s) { sounds[s]++; }
static void fake_save(const app_save_t *s) {
    saves++;
    saved = *s;
}
static void fake_mute(bool m) { (void)m; }
static bool fake_tilt(int *mg) {
    *mg = tilt_mg;
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
static const app_env_t ENV = {
    .song = fake_song,
    .song_stop = fake_song_stop,
    .song_playing = fake_song_playing,
    .play = fake_play,
    .save = fake_save,
    .mute = fake_mute,
    .tilt = fake_tilt,
    .accel = fake_accel,
    .orient = fake_orient,
};

static void reset_fakes(void) {
    memset(sounds, 0, sizeof(sounds));
    saves = 0;
    memset(&saved, 0, sizeof(saved));
    has_tilt = true;
    tilt_mg = 0;
    memset(raw_mg, 0, sizeof(raw_mg));
    memset(applied, 0, sizeof(applied));
    orients = 0;
    song_on = -1;
    muted_fake = false;
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

// Walks the menu to `it` (through GAMES for a game) and opens it.
static void open_item(app_t *a, app_item_t it, uint32_t *now) {
    app_open(a, *now);
    if (it >= ITEM_DICE) {
        while (a->item != ITEM_GAMES) ev(a, INPUT_BTN_B_SHORT, now);
        ev(a, INPUT_BTN_A_SHORT, now);
        assert(a->list == 1 && a->item == ITEM_DICE);
    }
    for (int k = 0; a->item != it; k++) {
        assert(k < ITEM_COUNT);
        ev(a, INPUT_BTN_B_SHORT, now);
    }
    ev(a, INPUT_BTN_A_SHORT, now);
    assert(a->mode == APP_GAME && a->item == it);
}

static void test_menu(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 1);
    assert(!app_active(&a));
    app_open(&a, now);
    assert(a.mode == APP_MENU && a.item == ITEM_FEED && a.list == 0);

    // B walks the top list and wraps; the slide animates, then stops.
    int visited = 0;
    do {
        ev(&a, INPUT_BTN_B_SHORT, &now);
        assert(app_is_animating(&a));
        run(&a, 300, &now);
        assert(!app_is_animating(&a));
        assert(a.item < ITEM_DICE);  // never strays into the games list
        visited++;
    } while (a.item != ITEM_FEED);
    assert(visited == 7);

    // GAMES opens the games list; its last item comes back to GAMES.
    while (a.item != ITEM_GAMES) ev(&a, INPUT_BTN_B_SHORT, &now);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(a.list == 1 && a.item == ITEM_DICE);
    visited = 0;
    while (a.item != ITEM_UP) {
        ev(&a, INPUT_BTN_B_SHORT, &now);
        assert(a.item >= ITEM_DICE);
        visited++;
    }
    assert(visited == 8);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(a.list == 0 && a.item == ITEM_GAMES && app_active(&a));

    // No accelerometer: Catch and Setup are not offered.
    has_tilt = false;
    app_close(&a);
    app_open(&a, now);
    for (int i = 0; i < ITEM_COUNT * 2; i++) {
        ev(&a, INPUT_BTN_B_SHORT, &now);
        assert(a.item != ITEM_SETUP);
    }
    while (a.item != ITEM_GAMES) ev(&a, INPUT_BTN_B_SHORT, &now);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    for (int i = 0; i < ITEM_COUNT * 2; i++) {
        ev(&a, INPUT_BTN_B_SHORT, &now);
        assert(a.item != ITEM_CATCH);
    }
    has_tilt = true;

    // Sound toggles and is saved.
    app_close(&a);
    app_open(&a, now);
    while (a.item != ITEM_SOUND) ev(&a, INPUT_BTN_B_SHORT, &now);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(a.save.muted && saves == 1 && saved.muted);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(!a.save.muted);

    // BACK closes; so does B long; so does doing nothing.
    while (a.item != ITEM_BACK) ev(&a, INPUT_BTN_B_SHORT, &now);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(!app_active(&a));
    app_open(&a, now);
    ev(&a, INPUT_BTN_B_LONG, &now);
    assert(!app_active(&a));
    app_open(&a, now);
    run(&a, APP_MENU_IDLE_MS + 100, &now);
    assert(!app_active(&a));

    // SLEEP closes the menu and asks for night, once.
    app_open(&a, now);
    while (a.item != ITEM_SLEEP) ev(&a, INPUT_BTN_B_SHORT, &now);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(!app_active(&a) && app_take_sleep(&a) && !app_take_sleep(&a));

    // In a game: B goes back to the menu (the games list), B long to the face.
    open_item(&a, ITEM_DICE, &now);
    ev(&a, INPUT_BTN_B_SHORT, &now);
    assert(a.mode == APP_MENU && a.list == 1 && a.item == ITEM_DICE);
    open_item(&a, ITEM_DICE, &now);
    ev(&a, INPUT_BTN_B_LONG, &now);
    assert(!app_active(&a));
}

static void test_dice_and_coin(void) {
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
        assert(a.g.dice.phase == DICE_SHOWN && (!app_is_animating(&a) || a.levelup));
        assert(a.g.dice.value >= 1 && a.g.dice.value <= 6);
        assert(a.g.dice.shown == a.g.dice.value);  // lands on what it rolled
        seen[a.g.dice.value]++;
    }
    for (int v = 1; v <= 6; v++) assert(seen[v] > 0);
    assert(sounds[SND_ROLL] == 120 && sounds[SND_DING] == 120);
    assert(a.save.pet.level > 1);  // 120 rolls of xp

    int heads = 0;
    ev(&a, INPUT_BTN_B_SHORT, &now);
    while (a.item != ITEM_COIN) ev(&a, INPUT_BTN_B_SHORT, &now);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    for (int k = 0; k < 100; k++) {
        ev(&a, INPUT_TAP, &now);
        run(&a, 1300, &now);
        assert(a.g.coin.phase == COIN_SHOWN);
        heads += a.g.coin.heads;
    }
    assert(heads > 25 && heads < 75);
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
    ev(&a, INPUT_BTN_B_SHORT, &now);
    open_item(&a, ITEM_CATCH, &now);
    run(&a, APP_GAME_IDLE_MS + 1000, &now);
    assert(!app_active(&a));
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

static void test_pet(void) {
    pet_t p;
    pet_init(&p);
    assert(p.level == 1 && !pet_needs(&p));
    // A day awake with no care: hungry, bored and tired.
    for (int m = 0; m < 24 * 60; m++) pet_minute(&p, false);
    assert(pet_needs(&p) == (PET_NEED_FOOD | PET_NEED_FUN | PET_NEED_SLEEP));
    assert(p.food == 0 && p.fun == 0 && p.energy == 0 && p.minutes == 24 * 60);
    // Sleep restores energy; boredom doesn't grow asleep.
    for (int m = 0; m < 60; m++) pet_minute(&p, true);
    assert(p.energy == PET_MAX && p.fun == 0);
    // Eats until full, then refuses.
    int meals = 0;
    while (pet_feed(&p, 35, 0)) meals++;
    assert(meals == 3 && p.food >= 95);
    // Levels: 30 xp leaves level 1, 40 leaves level 2.
    assert(!pet_add_xp(&p, 29) && pet_add_xp(&p, 1) && p.level == 2 && p.xp == 0);
    assert(pet_add_xp(&p, 45) && p.level == 3 && p.xp == 5);
}

static void test_pet_clock(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 6);
    assert(a.save.pet.level == 1);  // a fresh save gets a new pet
    // An hour of frames: 60 pet minutes, a save every 10.
    for (int k = 0; k <= 3600 * 5; k++) app_tick(&a, now += 200, false);  // the first starts the clock
    assert(a.save.pet.minutes == 60 && saves == 6 && saved.pet.minutes == 60);
    // A long stall (light sleep hiccup, debugger) doesn't age it by hours.
    app_tick(&a, now += 5u * 3600u * 1000u, false);
    assert(a.save.pet.minutes == 70);
    // Face interactions: petting cheers it up, shaking doesn't.
    a.save.pet.fun = 50;
    app_face_event(&a, INPUT_BTN_A_SHORT);
    app_face_event(&a, INPUT_DOUBLE_TAP);
    assert(a.save.pet.fun == 55);
    app_face_event(&a, INPUT_SHAKE);
    assert(a.save.pet.fun == 53);
    a.save.pet.food = 10;
    assert(app_needs(&a) == PET_NEED_FOOD);
}

static void test_feed(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 7);
    a.save.pet.food = 20;
    open_item(&a, ITEM_FEED, &now);
    assert(a.g.feed.phase == FEED_PICK && a.g.feed.choice == 0);

    // A fish: three chomps, YUM, the face will be in love.
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(a.g.feed.phase == FEED_EAT && a.save.pet.food == 55);
    run(&a, 1600, &now);
    assert(a.g.feed.phase == FEED_YUM && sounds[SND_EAT] == 3 && saves >= 1);
    assert(app_take_mood(&a) == EXPR_LOVE);
    run(&a, 1400, &now);
    assert(a.g.feed.phase == FEED_PICK);

    // B steps through the foods; cake is fun too.
    ev(&a, INPUT_BTN_B_SHORT, &now);
    ev(&a, INPUT_BTN_B_SHORT, &now);
    assert(a.g.feed.choice == 2 && a.mode == APP_GAME);  // B didn't leave
    const int fun = a.save.pet.fun;
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(a.save.pet.food == 75 && a.save.pet.fun == fun + 12);
    run(&a, 3000, &now);

    // Stuffed: refuses, nothing changes.
    a.save.pet.food = 99;
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(a.g.feed.phase == FEED_FULL && a.save.pet.food == 99);
    run(&a, 1400, &now);

    // The BACK choice returns to the menu.
    ev(&a, INPUT_BTN_B_SHORT, &now);
    assert(a.g.feed.choice == FOOD_KINDS);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(a.mode == APP_MENU && a.item == ITEM_FEED);
}

static void test_levelup_banner(void) {
    static app_t a;
    static gfx_t g;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 8);
    open_item(&a, ITEM_DICE, &now);
    a.save.pet.xp = (uint16_t)(pet_xp_next(1) - 1);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    run(&a, 900, &now);
    assert(a.save.pet.level == 2 && a.levelup && sounds[SND_LEVELUP] == 1);
    assert(app_is_animating(&a) && app_take_mood(&a) == EXPR_EXCITED);
    gfx_clear(&g);
    app_render(&a, &g);
    run(&a, 2000, &now);
    assert(!a.levelup && !app_is_animating(&a));
}

static void test_snake(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 9);
    open_item(&a, ITEM_SNAKE, &now);
    snake_t *s = &a.g.snake;
    assert(s->phase == SNAKE_START);
    // B on the title goes back to the menu.
    ev(&a, INPUT_BTN_B_PRESS, &now);
    ev(&a, INPUT_BTN_B_SHORT, &now);
    assert(a.mode == APP_MENU);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    press_a(&a, &now);
    assert(s->phase == SNAKE_PLAY && s->len == 4 && s->dir == 0);

    // Food right ahead: it eats and grows.
    s->food[0] = s->body[s->head][0] + 2;
    s->food[1] = s->body[s->head][1];
    run(&a, 350, &now);  // two steps (the berry it places next could be anywhere)
    assert(s->score == 1 && s->len == 5 && sounds[SND_POINT] == 1);

    // B turns right (down) and stays in the game; its SHORT is swallowed.
    ev(&a, INPUT_BTN_B_PRESS, &now);
    ev(&a, INPUT_BTN_B_SHORT, &now);
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

static void test_memory(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 10);
    open_item(&a, ITEM_MEMORY, &now);
    memory_t *m = &a.g.mem;
    press_a(&a, &now);
    assert(m->phase == MEM_SHOW && m->len == 1);

    // Play three rounds perfectly, pressing what was shown.
    for (int round = 1; round <= 3; round++) {
        int shown[MEMORY_MAX], n = 0, was = -1;
        while (m->phase == MEM_SHOW) {
            app_update(&a, now += FRAME);
            if (m->lit >= 0 && m->lit != was && m->phase == MEM_SHOW) shown[n++] = m->lit;
            was = m->lit;
            if (m->lit < 0) was = -1;
        }
        assert(m->phase == MEM_INPUT && n == round);
        for (int i = 0; i < n; i++) {
            assert(shown[i] == m->seq[i]);
            const bool right = shown[i] == 1;
            ev(&a, right ? INPUT_BTN_B_PRESS : INPUT_BTN_A_PRESS, &now);
            ev(&a, right ? INPUT_BTN_B_SHORT : INPUT_BTN_A_SHORT, &now);
            assert(a.mode == APP_GAME);
            run(&a, 250, &now);
        }
        assert(m->phase == MEM_GOOD && m->score == round);
        run(&a, 800, &now);
    }
    assert(m->len == 4);

    // A wrong press ends it.
    while (m->phase == MEM_SHOW) app_update(&a, now += FRAME);
    const bool right = m->seq[0] == 0;  // the wrong one
    ev(&a, right ? INPUT_BTN_B_PRESS : INPUT_BTN_A_PRESS, &now);
    assert(m->phase == MEM_OVER && m->score == 3 && saved.memory_pts == 3);
    // ...and that B's SHORT doesn't throw us out of the result screen.
    if (right) ev(&a, INPUT_BTN_B_SHORT, &now);
    assert(a.mode == APP_GAME);
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

static void test_render_all(void) {
    // Every screen draws something and nothing crashes (ASan watches).
    static app_t a;
    static gfx_t g;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 12);
    for (int it = ITEM_FEED; it < ITEM_COUNT; it++) {
        if (it == ITEM_GAMES || it == ITEM_SLEEP || it == ITEM_SOUND || it == ITEM_BACK || it == ITEM_UP) continue;
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
    // Every song parses, fits the player's buffer, and sits where the
    // buzzer is loud (no note below 1 kHz or above 4.5 kHz).
    static note_t big[320];
    assert(songs_count() == 7);
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
    int alarms = 0, glitches = 0, songs = 0, hangs = 0, last_song = -1;
    uint32_t last_event = 0;
    bool spaced = true;
    const char *first = NULL;
    // Eight hours, happy and idle: glitches and songs, never two close together.
    for (uint32_t t = 0; t < 8u * 3600u * 1000u; t += 200) {
        const story_event_t e = story_update(&s, now += 200, 0, true, true);
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
        alarms += e == STORY_ALARM;
        hangs += e == STORY_HANG;
    }
    printf("  8 h: %d glitches, %d songs, %d hangs, %d alarms\n", glitches, songs, hangs, alarms);
    assert(spaced && alarms == 0 && glitches >= 8 && glitches <= 35 && songs >= 5 && songs <= 20);
    assert(hangs >= 6 && hangs <= 25);
    assert(strcmp(first, "YETI-7 ONLINE") == 0);  // the story starts at the start

    // Hungry: an alarm about 10 s later, again 20 min later, none once fed.
    story_init(&s, 6, now);
    uint32_t t0 = now, a1 = 0, a2 = 0;
    for (int k = 0; k < 25 * 60 * 5; k++) {
        const story_event_t e = story_update(&s, now += 200, PET_NEED_FOOD, true, true);
        if (e == STORY_ALARM && !a1) a1 = now;
        else if (e == STORY_ALARM && !a2) a2 = now;
    }
    assert(a1 && a1 - t0 >= 10000 && a1 - t0 < 12000);
    assert(a2 && a2 - a1 >= 20u * 60u * 1000u);
    for (int k = 0; k < 30 * 60 * 5; k++) assert(story_update(&s, now += 200, 0, true, true) != STORY_ALARM);

    // Busy (menu open, night): nothing starts; it waits for idle.
    story_init(&s, 7, now);
    for (int k = 0; k < 2 * 3600 * 5; k++) assert(story_update(&s, now += 200, PET_NEED_FUN, false, true) == STORY_NONE);
    assert(story_update(&s, now += 200, PET_NEED_FUN, true, true) != STORY_NONE);
    // Muted: no songs, ever.
    story_init(&s, 8, now);
    for (int k = 0; k < 8 * 3600 * 5; k++) assert(story_update(&s, now += 200, 0, true, false) != STORY_SONG);
}

static void test_music(void) {
    static app_t a;
    uint32_t now = 1000;
    reset_fakes();
    app_init(&a, &ENV, NULL, 13);
    open_item(&a, ITEM_MUSIC, &now);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(song_on == 0 && a.g.music.playing && app_is_animating(&a));
    // Plays on without input (no idle timeout mid-song)...
    run(&a, APP_GAME_IDLE_MS + 1000, &now);
    assert(app_active(&a) && song_on == 0);
    // ...B goes to the next song and stops this one; A stops too.
    ev(&a, INPUT_BTN_B_SHORT, &now);
    assert(song_on == -1 && a.g.music.song == 1 && a.mode == APP_GAME);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(song_on == 1);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(song_on == -1);
    // Finished by itself: not playing any more.
    ev(&a, INPUT_BTN_A_SHORT, &now);
    song_on = -1;
    run(&a, 100, &now);
    assert(!a.g.music.playing);
    // Leaving with a song on stops it.
    ev(&a, INPUT_BTN_A_SHORT, &now);
    ev(&a, INPUT_BTN_B_LONG, &now);
    assert(!app_active(&a) && song_on == -1);
    // Muted: a fail beep, nothing plays.
    open_item(&a, ITEM_MUSIC, &now);
    muted_fake = true;
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(!a.g.music.playing && sounds[SND_FAIL] == 1);
    // The last choice is BACK.
    while (a.g.music.song != songs_count()) ev(&a, INPUT_BTN_B_SHORT, &now);
    ev(&a, INPUT_BTN_A_SHORT, &now);
    assert(a.mode == APP_MENU && a.item == ITEM_MUSIC);
}

int main(void) {
#define RUN(t) (printf("%s\n", #t), t())
    RUN(test_menu);
    RUN(test_dice_and_coin);
    RUN(test_reaction);
    RUN(test_catch);
    RUN(test_flappy);
    RUN(test_pet);
    RUN(test_pet_clock);
    RUN(test_feed);
    RUN(test_levelup_banner);
    RUN(test_snake);
    RUN(test_memory);
    RUN(test_setup);
    RUN(test_render_all);
    RUN(test_rtttl);
    RUN(test_story);
    RUN(test_music);
    printf("all app tests passed\n");
    return 0;
}
