// Music: the jukebox, B held on the face. It opens on the song played last
// time, silent: B steps through the songs, A plays the one showing. While
// it plays, one of the animations (music_viz.h) runs to its notes, a
// different one from last time. A stops (back to the list), B plays the
// next song at once, C stops; C in the list goes back to the face.

#include "app/app_internal.h"
#include "app/art.h"

#define TITLE_MS 2500     // the song's name, over the animation, at the start
#define MSG_MS 1500       // "SOUND OFF" when A finds the sound off
#define MAX_STEP_MS 100   // a long frame is not a long jump for the animation

static bool playing(const app_t *a) {
    return a->env && a->env->song_playing && a->env->song_playing();
}

static void stop(app_t *a) {
    if (a->g.music.playing && a->env && a->env->song_stop) a->env->song_stop();
    a->g.music.playing = false;
}

// A different animation from the last song's.
static viz_kind_t pick_viz(app_t *a) {
    int k;
    if (a->last_viz < 0 || a->last_viz >= VIZ_COUNT) {
        k = (int)rng_range(&a->rng, 0, VIZ_COUNT - 1);
    } else {
        k = (int)rng_range(&a->rng, 0, VIZ_COUNT - 2);
        if (k >= a->last_viz) k++;
    }
    a->last_viz = k;
    return (viz_kind_t)k;
}

// Plays the current song; false when the sound is off.
static bool start_song(app_t *a, uint32_t now) {
    music_t *m = &a->g.music;
    if (!(a->env && a->env->song && a->env->song(m->song))) {
        m->msg_until = now + MSG_MS;
        return false;
    }
    m->playing = true;
    m->t0 = m->last = now;
    m->next = 0;
    m->next_at = 0;
    m->msg_until = 0;
    a->last_song = m->song;
    viz_track_load(&m->track, m->song);
    viz_start(&m->viz, pick_viz(a), &a->rng);
    return true;
}

// Opens on the song played last time, not playing yet.
void music_start(app_t *a, uint32_t now) {
    music_t *m = &a->g.music;
    m->song = a->last_song >= 0 && a->last_song < songs_count() ? a->last_song : 0;
    m->playing = false;
    m->t0 = now;
}

void music_leave(app_t *a) {
    stop(a);
}

bool music_event(app_t *a, input_event_t ev, uint32_t now) {
    music_t *m = &a->g.music;
    if (m->playing) {
        switch (ev) {
            case INPUT_BTN_A_SHORT:  // stop: back to the list
            case INPUT_BTN_C_SHORT:
                stop(a);
                app_play(a, SND_BACK);
                return true;
            case INPUT_BTN_B_SHORT:  // the next song, playing at once
                stop(a);
                m->song = (m->song + 1) % songs_count();
                start_song(a, now);
                return true;
            default:
                return false;
        }
    }
    switch (ev) {
        case INPUT_BTN_A_SHORT:
            if (!start_song(a, now)) app_play(a, SND_FAIL);  // silent when the sound is off
            return true;
        case INPUT_BTN_B_SHORT:
            m->song = (m->song + 1) % songs_count();
            m->msg_until = 0;
            app_play(a, SND_MOVE);
            return true;
        default:
            return false;  // C: the app goes back to the face
    }
}

void music_update(app_t *a, uint32_t now) {
    music_t *m = &a->g.music;
    if (!m->playing) return;
    if (!playing(a)) {  // sung to the end: back to the list
        m->playing = false;
        return;
    }
    // Each note of the track as its moment comes.
    const uint32_t t = now - m->t0;
    while (m->next < m->track.n && m->next_at <= t) {
        if (m->track.notes[m->next].hz) viz_note(&m->viz, &m->track, m->next, &a->rng);
        m->next_at += m->track.notes[m->next].ms;
        m->next++;
    }
    int dt = (int)(now - m->last);
    if (dt > MAX_STEP_MS) dt = MAX_STEP_MS;
    if (dt > 0) viz_step(&m->viz, dt, &a->rng);
    m->last = now;
}

// The list: the song's picture, its name, a dot per song.
static void draw_chooser(const app_t *a, gfx_t *g) {
    const music_t *m = &a->g.music;
    const int n = songs_count();
    if (m->msg_until && !app_elapsed(a->now, m->msg_until, 0)) {
        font_draw_centered(g, 64, 16, "SOUND OFF", 2, GFX_WHITE);
    } else {
        art_song(g, song_name(m->song));
    }
    font_draw_centered(g, 64, 42, song_name(m->song), 1, GFX_WHITE);
    for (int i = 0; i < n; i++) {
        const int dx = 64 - n * 2 + i * 4;
        if (i == m->song) {
            gfx_fill_rect(g, dx, 51, 3, 2, GFX_WHITE);
        } else {
            gfx_pixel(g, dx + 1, 52, GFX_WHITE);
        }
    }
    app_hint(g, "PLAY", "NEXT", "BACK");
}

void music_render(const app_t *a, gfx_t *g) {
    const music_t *m = &a->g.music;
    if (!m->playing) {
        draw_chooser(a, g);
        return;
    }
    const uint32_t t = a->now - m->t0;
    viz_draw(&m->viz, &m->track, t, g);
    if (t < TITLE_MS) {  // the name, on a black strip along the bottom
        const char *name = song_name(m->song);
        const int w = font_width(name, 1) + 6;
        gfx_fill_rect(g, 64 - w / 2, 54, w, 10, GFX_BLACK);
        font_draw_centered(g, 64, 56, name, 1, GFX_WHITE);
    }
}

bool music_animating(const app_t *a) {
    return a->g.music.playing;
}
