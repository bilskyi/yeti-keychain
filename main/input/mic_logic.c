#include "input/mic_logic.h"

#include <stdlib.h>

#include "config.h"

#define BLOCK_MS (MIC_BLOCK * 1000 / MIC_SAMPLE_HZ)

static bool after(uint32_t now, uint32_t t) {
    return (int32_t)(now - t) >= 0;
}

void mic_init(mic_t *m) {
    *m = (mic_t){0};
}

void mic_deaf(mic_t *m, uint32_t until) {
    if (!m->deaf || (int32_t)(until - m->deaf_until) > 0) m->deaf_until = until;
    m->deaf = true;
    m->in_sound = false;
}

static int threshold(const mic_t *m) {
    const int t = (int)(m->floor * MIC_SNR / 16);
    return t > MIC_MIN_LEVEL ? t : MIC_MIN_LEVEL;
}

static void finish(mic_t *m, uint32_t ms, input_event_t ev) {
    m->in_sound = false;
    m->last = (mic_sound_t){
        .ms = (uint16_t)(ms > UINT16_MAX ? UINT16_MAX : ms),
        .peak = (uint16_t)m->peak,
        .mean = (uint16_t)(m->sum / m->blocks),
        .floor = (uint16_t)(m->floor / 16),
        .clip_pct = (uint8_t)(m->clip_blocks * 100 / m->blocks),
        .syllables = (uint8_t)(m->syllables > 9 ? 9 : m->syllables),
        .ev = ev,
    };
    m->has_last = true;
}

// The sound just ended: what was it?
static input_event_t classify(mic_t *m) {
    const uint32_t ms = m->last_loud - m->start + BLOCK_MS;
    const int clip_pct = m->clip_blocks * 100 / m->blocks;
    input_event_t ev = INPUT_NONE;
    if (ms <= MIC_CLAP_MAX_MS && m->peak >= MIC_CLAP_LEVEL) {
        if (m->clap_pending && (int32_t)(m->start - m->clap_at) <= MIC_DOUBLE_MS) {
            m->clap_pending = false;
            ev = INPUT_DOUBLE_CLAP;
        } else {
            m->clap_pending = true;  // a CLAP, unless a second one follows
            m->clap_at = m->start;
            finish(m, ms, INPUT_CLAP);
            return INPUT_NONE;
        }
    } else if (ms >= MIC_HEAR_MS) {
        ev = clip_pct >= MIC_BLOW_CLIP_PCT ? INPUT_BLOW
             : m->peak >= MIC_LOUD_LEVEL  ? INPUT_LOUD
                                          : INPUT_TALK;
    }
    finish(m, ms, ev);
    return ev;
}

input_event_t mic_block(mic_t *m, const uint16_t *s, int n, uint32_t now) {
    if (n <= 0) return INPUT_NONE;
    if (!m->primed) {
        int32_t sum = 0;
        for (int i = 0; i < n; i++) sum += s[i];
        m->dc = sum / n * 256;
        m->primed = true;
    }
    int32_t sum = 0;
    int clipped = 0;
    for (int i = 0; i < n; i++) {
        const int32_t d = (int32_t)s[i] * 256 - m->dc;
        m->dc += d / 1024;  // ~130 ms: follows the bias, not the sound
        sum += abs(d) / 256;
        clipped += s[i] < MIC_CLIP_LO || s[i] > MIC_CLIP_HI;
    }
    const int level = (int)(sum / n);

    if (m->deaf) {
        if (!after(now, m->deaf_until)) return INPUT_NONE;  // our own buzzer
        m->deaf = false;
    }
    if (m->floor == 0) m->floor = level * 16 + 16;

    const int thr = threshold(m);
    if (!m->in_sound) {
        // The noise floor drops fast and creeps up slowly (~5 s), so a
        // sound never raises it much but a noisy room does.
        const int32_t l16 = level * 16;
        if (l16 < m->floor) {
            m->floor -= (m->floor - l16) / 8 + 1;
        } else {
            m->floor += (l16 - m->floor + 511) / 512;  // rounded up: a small gap still closes
        }
        if (m->floor < 16) m->floor = 16;
        if (level <= thr) return INPUT_NONE;
        m->in_sound = true;
        m->start = m->last_loud = now;
        m->peak = 0;
        m->sum = 0;
        m->blocks = m->clip_blocks = 0;
        m->syllables = 1;
        m->syl_up = true;
        m->heard = false;
    }

    m->blocks++;
    m->sum += level;
    if (level > m->peak) m->peak = level;
    if (clipped * 8 >= n) m->clip_blocks++;
    if (level > thr) {
        m->last_loud = now;
        if (!m->syl_up) m->syllables++;
        m->syl_up = true;
    } else if (level < thr * 7 / 10) {
        m->syl_up = false;
    }

    if (now - m->start > MIC_TALK_MAX_MS) {
        // Music, a fan, a train: part of the room now, not something said.
        m->floor = m->sum / m->blocks * 16;
        finish(m, now - m->start, INPUT_NONE);
        return INPUT_NONE;
    }
    if ((int32_t)(now - m->last_loud) >= MIC_END_GAP_MS) return classify(m);
    if (!m->heard && now - m->start >= MIC_HEAR_MS && m->peak < MIC_LOUD_LEVEL &&
        m->clip_blocks * 100 < MIC_BLOW_CLIP_PCT * m->blocks) {
        m->heard = true;
        return INPUT_HEAR;
    }
    return INPUT_NONE;
}

input_event_t mic_poll(mic_t *m, uint32_t now) {
    if (!m->clap_pending || (int32_t)(now - m->clap_at) <= MIC_DOUBLE_MS) return INPUT_NONE;
    // A second clap that started in time is still being heard: wait for it.
    if (m->in_sound && (int32_t)(m->start - m->clap_at) <= MIC_DOUBLE_MS) return INPUT_NONE;
    m->clap_pending = false;
    return INPUT_CLAP;
}

bool mic_take_sound(mic_t *m, mic_sound_t *out) {
    if (!m->has_last) return false;
    *out = m->last;
    m->has_last = false;
    return true;
}

int mic_syllables(const mic_t *m) {
    return m->last.syllables ? m->last.syllables : 1;
}
