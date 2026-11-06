// The mood machine: which expression the face drifts into when nobody is
// interacting with it.
//
// A weighted random walk. Weights depend on the current mood (moods have some
// inertia and favour plausible neighbours) and on context: at night it is
// always sleepy, and the longer nobody has touched the keychain the more
// often it gets bored: sleepy, or a little sad.

#include "face/face.h"

#include "config.h"

// WEIGHTS[from][to]. Rows need not sum to anything in particular.
static const uint8_t WEIGHTS[EXPR_MOOD_COUNT][EXPR_MOOD_COUNT] = {
    //              NEU  HAP  SLP  SUR  ANG  LOV  SAD
    [EXPR_NEUTRAL]   = {30, 30, 10,  8,  4, 10,  3},
    [EXPR_HAPPY]     = {40, 20,  5,  8,  2, 15,  1},
    [EXPR_SLEEPY]    = {40, 10, 30, 10,  4,  2,  4},
    [EXPR_SURPRISED] = {35, 35,  5,  0,  8, 10,  2},
    [EXPR_ANGRY]     = {50, 15,  5, 15,  0,  5,  5},
    [EXPR_LOVE]      = {40, 40,  5,  5,  0,  5,  1},
    [EXPR_SAD]       = {45, 15, 15,  5,  5,  5, 10},
};

// Ignored for this long, the keychain starts getting sleepy and a bit sad.
#define BORED_AFTER_MS (5UL * 60UL * 1000UL)

expression_t mood_pick(rng_t *rng, expression_t current, const mood_ctx_t *ctx,
                       uint32_t *hold_ms) {
    *hold_ms = rng_range(rng, MOOD_MIN_MS, MOOD_MAX_MS);
    if (ctx->night) return EXPR_SLEEPY;

    if (current >= EXPR_MOOD_COUNT) current = EXPR_NEUTRAL;

    uint32_t w[EXPR_MOOD_COUNT];
    uint32_t total = 0;
    for (int i = 0; i < EXPR_MOOD_COUNT; i++) {
        w[i] = WEIGHTS[current][i];
        if ((i == EXPR_SLEEPY || i == EXPR_SAD) && ctx->ms_since_input >= BORED_AFTER_MS) w[i] *= 3;
        total += w[i];
    }

    uint32_t roll = rng_range(rng, 0, total - 1);
    for (int i = 0; i < EXPR_MOOD_COUNT; i++) {
        if (roll < w[i]) return (expression_t)i;
        roll -= w[i];
    }
    return EXPR_NEUTRAL;
}
