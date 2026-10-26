// Music: the jukebox. B picks a song (the last choice is BACK), A plays or
// stops it. The Yeti sings along.

#include "app/app_internal.h"
#include "sound/rtttl.h"

static bool playing(const app_t *a) {
    return a->env && a->env->song_playing && a->env->song_playing();
}

static void stop(app_t *a) {
    if (a->g.music.playing && a->env && a->env->song_stop) a->env->song_stop();
    a->g.music.playing = false;
}

void music_start(app_t *a, uint32_t now) {
    a->g.music.song = 0;
    a->g.music.playing = false;
    a->g.music.t0 = now;
}

void music_leave(app_t *a) {
    stop(a);
}

bool music_event(app_t *a, input_event_t ev, uint32_t now) {
    music_t *m = &a->g.music;
    if (ev == INPUT_BTN_B_SHORT) {
        stop(a);
        m->song = (m->song + 1) % (songs_count() + 1);
        app_play(a, SND_MOVE);
        return true;
    }
    if (ev != INPUT_BTN_A_SHORT) return false;
    if (m->song == songs_count()) {  // BACK
        app_play(a, SND_BACK);
        a->mode = APP_MENU;
        return true;
    }
    if (m->playing) {
        stop(a);
    } else if (a->env && a->env->song && a->env->song(m->song)) {
        m->playing = true;
        m->t0 = now;
    } else {
        app_play(a, SND_FAIL);  // muted: nothing to hear
    }
    return true;
}

void music_update(app_t *a, uint32_t now) {
    music_t *m = &a->g.music;
    if (m->playing && !playing(a)) {  // finished
        m->playing = false;
        app_reward(a, 2, 0, 1);
    }
    (void)now;
}

void music_render(const app_t *a, gfx_t *g) {
    const music_t *m = &a->g.music;
    const bool back = m->song == songs_count();
    if (m->playing) {
        // Singing along: happy eyes, mouth on the beat, notes floating up.
        const uint32_t t = a->now - m->t0;
        const int bob = (int)(t / 350 % 2);
        for (int k = 0; k < 3; k++) {
            gfx_line(g, 36 + k, 22 + bob, 44, 15 + bob, GFX_WHITE);
            gfx_line(g, 44, 15 + bob + k, 52 - k, 22 + bob, GFX_WHITE);
            gfx_line(g, 76 + k, 22 + bob, 84, 15 + bob, GFX_WHITE);
            gfx_line(g, 84, 15 + bob + k, 92 - k, 22 + bob, GFX_WHITE);
        }
        if ((t / 180) % 3) {
            gfx_fill_ellipse(g, 64, 30 + bob, 6, 5, GFX_WHITE);
            gfx_fill_rect(g, 57, 24 + bob, 15, 6, GFX_BLACK);
        } else {
            gfx_fill_rect(g, 59, 29 + bob, 11, 2, GFX_WHITE);
        }
        for (int k = 0; k < 2; k++) {
            const int q = (int)(t / 50 + (uint32_t)k * 12) % 24;
            const int x = k ? 104 + q / 4 : 18 - q / 4, y = 34 - q;
            gfx_fill_circle(g, x, y, 2, GFX_WHITE);
            gfx_fill_rect(g, x + 2, y - 7, 1, 8, GFX_WHITE);
        }
    } else {
        // A note icon.
        gfx_fill_circle(g, 56, 24, 5, GFX_WHITE);
        gfx_fill_circle(g, 74, 20, 5, GFX_WHITE);
        gfx_fill_rect(g, 60, 6, 2, 18, GFX_WHITE);
        gfx_fill_rect(g, 78, 2, 2, 18, GFX_WHITE);
        gfx_fill_triangle(g, 60, 6, 80, 2, 80, 6, GFX_WHITE);
        gfx_fill_triangle(g, 60, 6, 60, 10, 80, 6, GFX_WHITE);
    }
    font_draw_centered(g, 64, 41, back ? "BACK" : song_name(m->song), 1, GFX_WHITE);
    app_hint(g, back ? "A:OK" : m->playing ? "A:STOP" : "A:PLAY", "B:NEXT");
}

bool music_animating(const app_t *a) {
    return a->g.music.playing;
}
