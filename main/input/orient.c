#include "input/orient.h"

#include <stdlib.h>

#define CLEAR_MG 700  // the dominant axis must read at least this (~45 degrees)

void orient_apply(const int8_t map[3], const int in[3], int out[3]) {
    if (!map[0] && !map[1] && !map[2]) {
        for (int i = 0; i < 3; i++) out[i] = in[i];
        return;
    }
    for (int i = 0; i < 3; i++) {
        const int m = map[i];
        const int axis = abs(m) - 1;
        out[i] = (axis < 0 || axis > 2) ? 0 : (m < 0 ? -in[axis] : in[axis]);
    }
}

// The axis gravity is along, with its sign; false when none is clear.
static bool dominant(const int v[3], int *axis, int *sign) {
    int best = 0;
    for (int i = 1; i < 3; i++) {
        if (abs(v[i]) > abs(v[best])) best = i;
    }
    if (abs(v[best]) < CLEAR_MG) return false;
    *axis = best;
    *sign = v[best] < 0 ? -1 : 1;
    return true;
}

bool orient_solve(const int flat[3], const int upright[3], int8_t map[3]) {
    int an, sn, au, su;
    if (!dominant(flat, &an, &sn) || !dominant(upright, &au, &su) || an == au) return false;
    // right = up x out. For basis vectors e_i x e_j = +e_k when (i, j, k) is
    // in cyclic order (x, y, z), -e_k otherwise.
    const int ak = 3 - au - an;
    const int eps = (an - au + 3) % 3 == 1 ? 1 : -1;
    const int sr = su * sn * eps;
    map[0] = (int8_t)((ak + 1) * sr);
    map[1] = (int8_t)((au + 1) * su);
    map[2] = (int8_t)((an + 1) * sn);
    return true;
}
