// xorshift32: tiny, seedable, deterministic. Good for animation, not crypto.

#pragma once

#include <stdint.h>

typedef struct {
    uint32_t s;
} rng_t;

static inline void rng_seed(rng_t *r, uint32_t seed) {
    r->s = seed ? seed : 0x9E3779B9u;  // xorshift's state must never be 0
}

static inline uint32_t rng_next(rng_t *r) {
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return r->s = x;
}

// Uniform in [lo, hi], inclusive. Modulo bias is irrelevant at these ranges.
static inline uint32_t rng_range(rng_t *r, uint32_t lo, uint32_t hi) {
    return hi <= lo ? lo : lo + rng_next(r) % (hi - lo + 1);
}

// True with probability pct/100.
static inline int rng_chance(rng_t *r, uint32_t pct) {
    return rng_next(r) % 100 < pct;
}
