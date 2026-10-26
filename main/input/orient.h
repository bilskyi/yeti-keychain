// Board axes -> screen axes. The LIS3DH can sit in the case any way round;
// a two-step calibration (lay it flat, hold it upright) finds which board axis
// is which, and from then on every sample is rotated into the screen frame:
//   x = right, y = up, z = out of the screen (toward you).
// At rest the sensor reads the "up" direction, so upright reads y = +1 g and
// lying flat face up reads z = +1 g.
//
// A map entry is (board axis + 1) * sign, so +-1..+-3; all zero = not
// calibrated, which means board axes as they are.
//
// Pure C (host-tested).

#pragma once

#include <stdbool.h>
#include <stdint.h>

void orient_apply(const int8_t map[3], const int in[3], int out[3]);

// From a sample lying flat face up and one held upright, both raw. False when
// either is not clearly along one axis, or both are along the same one.
bool orient_solve(const int flat[3], const int upright[3], int8_t map[3]);
