// The pictures: one for each game in the games list, one for each song in
// the jukebox. Just a picture and the name under it (Sasha: "keep it
// simple"). Each fills the top of the screen, x 4..119 (the list's position
// dots are at 123), y 0..38; the name goes below, the hints at the bottom.

#include "app/art.h"

#include <string.h>

#include "app/app_internal.h"

#define W GFX_W

static void pip(gfx_t *g, int x, int y, int r) {
    gfx_fill_circle(g, x, y, r, GFX_BLACK);
}

static void flake(gfx_t *g, int x, int y, int r) {
    gfx_hline(g, x - r, x + r, y, GFX_WHITE);
    gfx_line(g, x, y - r, x, y + r, GFX_WHITE);
    gfx_line(g, x - r + 1, y - r + 1, x + r - 1, y + r - 1, GFX_WHITE);
    gfx_line(g, x - r + 1, y + r - 1, x + r - 1, y - r + 1, GFX_WHITE);
}

static void sparkle(gfx_t *g, int x, int y, int r) {
    gfx_hline(g, x - r, x + r, y, GFX_WHITE);
    gfx_line(g, x, y - r, x, y + r, GFX_WHITE);
}

static void stars(gfx_t *g, const int8_t (*at)[2], int n) {
    for (int i = 0; i < n; i++) {
        if (i % 3 == 0) {
            sparkle(g, at[i][0], at[i][1], 1);
        } else {
            gfx_pixel(g, at[i][0], at[i][1], GFX_WHITE);
        }
    }
}

// A die s px square, black-edged so two can overlap.
static void die(gfx_t *g, int x, int y, int s, int face) {
    gfx_fill_rrect(g, x - 2, y - 2, s + 4, s + 4, 6, GFX_BLACK);
    gfx_fill_rrect(g, x, y, s, s, 5, GFX_WHITE);
    const int r = s / 10 + 1;
    const int a = x + s / 4 + 1, b = x + s / 2, c = x + s - s / 4 - 1;
    const int d = y + s / 4 + 1, e = y + s / 2, f = y + s - s / 4 - 1;
    pip(g, a, d, r);
    pip(g, c, f, r);
    if (face == 3) pip(g, b, e, r);
    if (face >= 4) {
        pip(g, c, d, r);
        pip(g, a, f, r);
    }
    if (face == 5) pip(g, b, e, r);
}

// --- the dinosaur, shared with the game ---

void app_dino(gfx_t *g, int x, int y, int pose, bool roaring, int scale) {
    static const char *const BODY[13] = {
        ".........#######", "........##.#####", "........########", "........########",
        "........#####...", "........#######.", "#......#####....", "#.....######....",
        "##...#########..", "###.#######.#...", "###########.....", ".#########......",
        "..#######.......",
    };
    static const char *const FEET[3][2] = {
        {"...##..##.......", "...#....##......"},
        {"...###.#........", "........#......."},
        {"...###.##.......", "....##..##......"},
    };
    const int s = scale < 1 ? 1 : scale;
    for (int r = 0; r < 15; r++) {
        const char *row = r < 13 ? BODY[r] : FEET[pose < 0 || pose > 2 ? 0 : pose][r - 13];
        if (roaring && r == 4) row = "........###.....";  // the jaw drops
        if (roaring && r == 5) row = "........####....";
        for (int c = 0; c < 16; c++) {
            if (row[c] == '#') gfx_fill_rect(g, x + c * s, y + r * s, s, s, GFX_WHITE);
        }
    }
}

void app_roar(gfx_t *g, int x, int y, int n) {
    for (int k = 0; k < n; k++) {
        const int r = 3 + k * 3;
        gfx_line(g, x + r - 1, y - r, x + r + 1, y - r / 3, GFX_WHITE);
        gfx_line(g, x + r + 1, y - r / 3, x + r + 1, y + r / 3, GFX_WHITE);
        gfx_line(g, x + r + 1, y + r / 3, x + r - 1, y + r, GFX_WHITE);
    }
}

// --- games ---

static void art_catch(gfx_t *g) {
    static const int8_t F[][3] = {{14, 6, 3}, {34, 16, 2}, {52, 4, 2}, {80, 9, 3}, {104, 18, 2}, {112, 5, 2},
                                  {24, 26, 2}, {96, 28, 1}};
    for (unsigned i = 0; i < sizeof(F) / sizeof(F[0]); i++) flake(g, F[i][0], F[i][1], F[i][2]);
    gfx_fill_circle(g, 78, 22, 4, GFX_WHITE);  // a rock, coming down fast
    for (int k = 0; k < 3; k++) gfx_line(g, 75 + k * 3, 10 + k, 75 + k * 3, 15 + k, GFX_WHITE);
    gfx_fill_rrect(g, 50, 24, 28, 14, 6, GFX_WHITE);  // the Yeti, arms up to catch
    app_mini_face(g, 64, 30, 5, 2, GFX_BLACK);
    for (int k = 0; k < 2; k++) {
        gfx_line(g, 51 + k, 27, 43 + k, 18, GFX_WHITE);
        gfx_line(g, 76 + k, 27, 84 + k, 18, GFX_WHITE);
    }
    gfx_hline(g, 8, 119, 38, GFX_WHITE);
}

static void art_maze(gfx_t *g) {
    gfx_rect(g, 22, 1, 84, 37, GFX_WHITE);
    gfx_rect(g, 23, 2, 82, 35, GFX_WHITE);
    gfx_fill_rect(g, 23, 1, 10, 2, GFX_BLACK);  // the way in
    static const int8_t WALLS[][4] = {
        {36, 2, 36, 26}, {36, 26, 52, 26}, {48, 12, 76, 12}, {64, 12, 64, 37},
        {76, 12, 76, 26}, {76, 26, 92, 26}, {88, 2, 88, 18}, {50, 26, 50, 37},
    };
    for (unsigned i = 0; i < sizeof(WALLS) / sizeof(WALLS[0]); i++) {
        gfx_fill_rect(g, WALLS[i][0], WALLS[i][1], WALLS[i][2] - WALLS[i][0] + 2, WALLS[i][3] - WALLS[i][1] + 2, GFX_WHITE);
    }
    gfx_fill_circle(g, 29, 8, 3, GFX_WHITE);  // the ball...
    for (int k = 0; k < 4; k++) gfx_pixel(g, 29, 14 + k * 4, GFX_WHITE);
    gfx_fill_circle(g, 97, 30, 5, GFX_WHITE);  // ...and the ring it rolls to
    gfx_fill_circle(g, 97, 30, 3, GFX_BLACK);
}

static void art_flappy(gfx_t *g) {
    for (int p = 0; p < 2; p++) {
        const int x = 70 + p * 32, gap = p ? 22 : 14;
        gfx_fill_rect(g, x, 0, 12, gap - 8, GFX_WHITE);
        gfx_fill_rect(g, x - 2, gap - 9, 16, 4, GFX_WHITE);
        gfx_fill_rect(g, x, gap + 9, 12, 38 - gap - 8, GFX_WHITE);
        gfx_fill_rect(g, x - 2, gap + 7, 16, 4, GFX_WHITE);
    }
    gfx_fill_circle(g, 40, 18, 9, GFX_WHITE);  // the bird, mid-flap
    gfx_fill_circle(g, 38, 15, 2, GFX_BLACK);
    gfx_fill_circle(g, 45, 15, 2, GFX_BLACK);
    gfx_fill_rect(g, 40, 21, 5, 2, GFX_BLACK);
    gfx_fill_triangle(g, 20, 8, 33, 17, 20, 24, GFX_WHITE);
    for (int k = 0; k < 4; k++) gfx_pixel(g, 8 + k * 4, 34 - k * 4, GFX_WHITE);  // its way up
}

static void art_snake(gfx_t *g) {
    static const int8_t PATH[][2] = {{0, 6}, {1, 6}, {2, 6}, {3, 6}, {3, 5}, {3, 4}, {3, 3}, {4, 3}, {5, 3},
                                     {6, 3}, {7, 3}, {7, 2}, {7, 1}, {8, 1}, {9, 1}, {10, 1}, {11, 1}, {11, 2},
                                     {11, 3}, {12, 3}, {13, 3}, {14, 3}};
    const int n = (int)(sizeof(PATH) / sizeof(PATH[0]));
    for (int i = 0; i < n; i++) gfx_fill_rect(g, 8 + PATH[i][0] * 6, 2 + PATH[i][1] * 5, 5, 4, GFX_WHITE);
    const int hx = 8 + 14 * 6, hy = 2 + 3 * 5;
    gfx_fill_rrect(g, hx, hy - 1, 7, 6, 2, GFX_WHITE);  // the head, an eye, the tongue
    gfx_pixel(g, hx + 4, hy, GFX_BLACK);
    gfx_hline(g, hx + 7, hx + 9, hy + 2, GFX_WHITE);
    gfx_pixel(g, hx + 10, hy + 1, GFX_WHITE);
    gfx_pixel(g, hx + 10, hy + 3, GFX_WHITE);
    gfx_fill_circle(g, 113, 28, 4, GFX_WHITE);  // an apple
    gfx_line(g, 113, 23, 115, 20, GFX_WHITE);
}

static void art_dino(gfx_t *g) {
    app_dino(g, 14, 8, 0, true, 2);
    app_roar(g, 46, 20, 3);
    gfx_fill_rrect(g, 88, 10, 7, 28, 3, GFX_WHITE);  // a cactus
    gfx_fill_rect(g, 82, 16, 3, 10, GFX_WHITE);
    gfx_fill_rect(g, 82, 24, 7, 3, GFX_WHITE);
    gfx_fill_rect(g, 98, 13, 3, 10, GFX_WHITE);
    gfx_fill_rect(g, 94, 21, 7, 3, GFX_WHITE);
    gfx_hline(g, 4, 119, 38, GFX_WHITE);
    for (int k = 0; k < 6; k++) gfx_pixel(g, 10 + k * 19, 36 + k % 2, GFX_WHITE);  // pebbles
}

static void art_reaction(gfx_t *g) {
    // A stopwatch, and a bolt either side.
    gfx_fill_circle(g, 64, 21, 16, GFX_WHITE);
    gfx_fill_circle(g, 64, 21, 13, GFX_BLACK);
    gfx_fill_rect(g, 60, 1, 9, 3, GFX_WHITE);
    gfx_fill_rect(g, 63, 3, 3, 3, GFX_WHITE);
    for (int k = 0; k < 12; k++) {
        const int a = k * 64 / 12;
        gfx_pixel(g, 64 + app_sin64(a + 16) * 11 / 256, 21 + app_sin64(a) * 11 / 256, GFX_WHITE);
    }
    gfx_line(g, 64, 21, 64, 12, GFX_WHITE);
    gfx_line(g, 65, 21, 65, 12, GFX_WHITE);
    gfx_line(g, 64, 21, 71, 25, GFX_WHITE);
    gfx_fill_circle(g, 64, 21, 2, GFX_WHITE);
    for (int s = -1; s <= 1; s += 2) {
        const int cx = 64 + s * 36;
        gfx_fill_triangle(g, cx + 3, 6, cx - 6, 22, cx + 1, 22, GFX_WHITE);
        gfx_fill_triangle(g, cx - 3, 36, cx + 6, 18, cx - 1, 18, GFX_WHITE);
    }
}

static void art_dice(gfx_t *g) {
    die(g, 30, 4, 30, 5);
    die(g, 62, 10, 26, 3);
    for (int k = 0; k < 3; k++) gfx_hline(g, 8 + k * 3, 22, 14 + k * 8, GFX_WHITE);  // rolling in
    sparkle(g, 100, 8, 2);
    sparkle(g, 108, 30, 1);
}

// Pong across two screens: the near paddle, the far one at the top edge
// beyond the net, and the ball on its way over.
static void art_pong(gfx_t *g) {
    for (int x = 4; x < 124; x += 6) gfx_hline(g, x, x + 2, 14, GFX_WHITE);  // the net
    gfx_fill_rrect(g, 70, 3, 22, 4, 2, GFX_WHITE);                           // BIT's paddle, far off
    gfx_fill_rrect(g, 34, 33, 30, 5, 2, GFX_WHITE);                          // mine
    gfx_fill_circle(g, 76, 20, 3, GFX_WHITE);
    for (int k = 1; k <= 4; k++) gfx_pixel(g, 76 - k * 4, 20 + k * 3, GFX_WHITE);  // where it came from
}

// BIT on its desk - a screen with two eyes - and a heart flying over to it.
static void art_bit(gfx_t *g) {
    gfx_fill_rrect(g, 70, 6, 46, 30, 6, GFX_WHITE);
    gfx_fill_rrect(g, 74, 10, 38, 22, 4, GFX_BLACK);
    gfx_fill_rrect(g, 81, 14, 7, 12, 3, GFX_WHITE);
    gfx_fill_rrect(g, 98, 14, 7, 12, 3, GFX_WHITE);
    gfx_fill_rect(g, 88, 36, 10, 2, GFX_WHITE);  // its stand
    app_heart(g, 30, 22, 16);
    for (int k = 0; k < 3; k++) gfx_pixel(g, 44 + k * 6, 18 - k * 2, GFX_WHITE);  // on its way
}

void art_game(gfx_t *g, app_item_t it) {
    switch (it) {
        case ITEM_CATCH: art_catch(g); break;
        case ITEM_MAZE: art_maze(g); break;
        case ITEM_FLAPPY: art_flappy(g); break;
        case ITEM_SNAKE: art_snake(g); break;
        case ITEM_VOICE: art_dino(g); break;
        case ITEM_REACTION: art_reaction(g); break;
        case ITEM_DICE: art_dice(g); break;
        case ITEM_PONG: art_pong(g); break;
        case ITEM_BIT: art_bit(g); break;
        default: break;
    }
}

// --- songs ---

static const int8_t SKY[][2] = {{8, 5}, {20, 30}, {30, 12}, {98, 6}, {110, 24}, {116, 10}, {14, 20}, {104, 34}, {90, 16}};
#define SKY_N ((int)(sizeof(SKY) / sizeof(SKY[0])))

static void art_barbie(gfx_t *g) {  // a big heart in sparkles
    app_heart(g, 64, 18, 34);
    gfx_fill_circle(g, 55, 9, 2, GFX_BLACK);  // its shine
    app_heart(g, 26, 26, 12);
    app_heart(g, 100, 10, 10);
    sparkle(g, 92, 30, 2);
    sparkle(g, 18, 8, 2);
    sparkle(g, 110, 26, 1);
}

static void art_countdown(gfx_t *g) {  // a rocket lifting off
    stars(g, SKY, SKY_N);
    gfx_fill_triangle(g, 64, 0, 57, 10, 71, 10, GFX_WHITE);
    gfx_fill_rect(g, 57, 10, 15, 16, GFX_WHITE);
    gfx_fill_circle(g, 64, 15, 3, GFX_BLACK);  // the window
    gfx_fill_triangle(g, 57, 18, 50, 28, 57, 26, GFX_WHITE);  // fins
    gfx_fill_triangle(g, 71, 18, 78, 28, 71, 26, GFX_WHITE);
    gfx_fill_triangle(g, 59, 27, 69, 27, 64, 37, GFX_WHITE);  // the flame
    gfx_fill_triangle(g, 62, 27, 66, 27, 64, 32, GFX_BLACK);
    for (int k = 0; k < 3; k++) {  // smoke
        gfx_fill_circle(g, 46 - k * 10, 35 - k, 4 - k, GFX_WHITE);
        gfx_fill_circle(g, 82 + k * 10, 35 - k, 4 - k, GFX_WHITE);
    }
}

static void art_takeonme(gfx_t *g) {  // a pencil sketching, like the video
    for (int k = 0; k < 2; k++) gfx_rect(g, 14 + k, 3 + k * 2, 66, 32, GFX_WHITE);  // a rough double frame
    for (int x = 20; x < 70; x += 6) gfx_line(g, x, 31, x + 8, 23, GFX_WHITE);  // hatching
    gfx_fill_circle(g, 40, 15, 6, GFX_WHITE);  // a face in the sketch
    gfx_fill_circle(g, 40, 15, 5, GFX_BLACK);
    gfx_pixel(g, 38, 14, GFX_WHITE);
    gfx_pixel(g, 42, 14, GFX_WHITE);
    gfx_hline(g, 38, 42, 18, GFX_WHITE);
    // The pencil, its tip on the drawing.
    for (int k = -2; k <= 2; k++) gfx_line(g, 112 + k, 2 - k, 84 + k, 30 - k, GFX_WHITE);
    gfx_fill_triangle(g, 82, 26, 88, 32, 78, 36, GFX_WHITE);
    gfx_line(g, 108, 2, 116, 10, GFX_BLACK);
    gfx_line(g, 60, 10, 78, 34, GFX_WHITE);  // the line it is drawing
}

static void stick(gfx_t *g, int x, int dir, bool kick) {
    gfx_fill_circle(g, x, 8, 4, GFX_WHITE);
    gfx_line(g, x, 12, x, 25, GFX_WHITE);
    gfx_line(g, x + 1, 12, x + 1, 25, GFX_WHITE);
    gfx_line(g, x, 15, x + dir * 10, 12, GFX_WHITE);  // punching arm
    gfx_line(g, x, 16, x - dir * 7, 21, GFX_WHITE);
    if (kick) {
        gfx_line(g, x, 25, x + dir * 14, 19, GFX_WHITE);
        gfx_line(g, x, 26, x + dir * 14, 20, GFX_WHITE);
    } else {
        gfx_line(g, x, 25, x + dir * 6, 37, GFX_WHITE);
    }
    gfx_line(g, x, 25, x - dir * 6, 37, GFX_WHITE);
}

static void art_kombat(gfx_t *g) {  // two fighters, a clash between them
    stick(g, 38, 1, true);
    stick(g, 90, -1, false);
    for (int k = 0; k < 8; k++) {
        const int a = k * 8;
        gfx_line(g, 64 + app_sin64(a + 16) * 3 / 256, 16 + app_sin64(a) * 3 / 256, 64 + app_sin64(a + 16) * 9 / 256,
                 16 + app_sin64(a) * 9 / 256, GFX_WHITE);
    }
    gfx_hline(g, 20, 108, 38, GFX_WHITE);
}

static void block(gfx_t *g, int c, int r) {
    gfx_fill_rect(g, 34 + c * 6, 3 + r * 6, 5, 5, GFX_WHITE);
    gfx_pixel(g, 34 + c * 6 + 1, 3 + r * 6 + 1, GFX_BLACK);
}

static void art_korobeiniki(gfx_t *g) {  // Tetris: a T on its way down to the stack
    static const int8_t STACK[][2] = {{0, 5}, {1, 5}, {2, 5}, {3, 5}, {5, 5}, {6, 5}, {7, 5}, {8, 5}, {9, 5},
                                      {0, 4}, {1, 4}, {5, 4}, {6, 4}, {8, 4}, {9, 4}, {0, 3}, {9, 3}, {8, 3}};
    for (unsigned i = 0; i < sizeof(STACK) / sizeof(STACK[0]); i++) block(g, STACK[i][0], STACK[i][1]);
    block(g, 3, 0);
    block(g, 4, 0);
    block(g, 5, 0);
    block(g, 4, 1);
    gfx_line(g, 31, 2, 31, 38, GFX_WHITE);  // the well
    gfx_line(g, 95, 2, 95, 38, GFX_WHITE);
}

static void art_potter(gfx_t *g) {  // the round glasses and the scar
    for (int s = -1; s <= 1; s += 2) {
        gfx_fill_circle(g, 64 + s * 17, 26, 11, GFX_WHITE);
        gfx_fill_circle(g, 64 + s * 17, 26, 8, GFX_BLACK);
        gfx_line(g, 64 + s * 28, 24, 64 + s * 38, 20, GFX_WHITE);  // the arms
        gfx_line(g, 64 + s * 28, 25, 64 + s * 38, 21, GFX_WHITE);
    }
    gfx_fill_rect(g, 58, 23, 12, 2, GFX_WHITE);  // the bridge
    gfx_line(g, 68, 0, 60, 7, GFX_WHITE);  // the lightning scar
    gfx_line(g, 69, 0, 61, 7, GFX_WHITE);
    gfx_line(g, 60, 7, 68, 8, GFX_WHITE);
    gfx_line(g, 68, 8, 60, 15, GFX_WHITE);
    gfx_line(g, 69, 8, 61, 15, GFX_WHITE);
}

static void art_imperial(gfx_t *g) {  // a battle station in the stars
    stars(g, SKY, SKY_N);
    gfx_fill_circle(g, 64, 20, 18, GFX_WHITE);
    gfx_hline(g, 46, 82, 19, GFX_BLACK);  // the trench
    gfx_fill_circle(g, 56, 11, 5, GFX_BLACK);  // the dish
    gfx_fill_circle(g, 56, 11, 2, GFX_WHITE);
    for (int k = 0; k < 4; k++) gfx_hline(g, 60 + k * 3, 66 + k * 4, 26 + k * 3, GFX_BLACK);  // panels
    // A little fighter: two wings and a ball.
    gfx_fill_rect(g, 100, 25, 2, 9, GFX_WHITE);
    gfx_fill_rect(g, 110, 25, 2, 9, GFX_WHITE);
    gfx_hline(g, 101, 110, 29, GFX_WHITE);
    gfx_fill_circle(g, 105, 29, 2, GFX_WHITE);
}

static void bell(gfx_t *g, int cx, int cy) {
    gfx_fill_circle(g, cx, cy - 6, 7, GFX_WHITE);
    gfx_fill_triangle(g, cx - 7, cy - 6, cx + 7, cy - 6, cx - 11, cy + 8, GFX_WHITE);
    gfx_fill_triangle(g, cx + 7, cy - 6, cx + 11, cy + 8, cx - 11, cy + 8, GFX_WHITE);
    gfx_fill_rect(g, cx - 12, cy + 7, 25, 3, GFX_WHITE);
    gfx_fill_circle(g, cx, cy + 12, 2, GFX_WHITE);
}

static void art_jingle(gfx_t *g) {  // two bells under a bow, ringing
    bell(g, 50, 22);
    bell(g, 78, 22);
    gfx_fill_triangle(g, 64, 6, 52, 0, 52, 11, GFX_WHITE);  // the bow
    gfx_fill_triangle(g, 64, 6, 76, 0, 76, 11, GFX_WHITE);
    gfx_fill_circle(g, 64, 6, 3, GFX_WHITE);
    for (int k = 0; k < 3; k++) {
        gfx_line(g, 30 - k * 4, 14 + k * 6, 34 - k * 4, 14 + k * 6, GFX_WHITE);
        gfx_line(g, 94 + k * 4, 14 + k * 6, 98 + k * 4, 14 + k * 6, GFX_WHITE);
    }
}

static void art_christmas(gfx_t *g) {  // a present with a heart on its tag
    gfx_fill_rect(g, 44, 14, 40, 24, GFX_WHITE);
    gfx_fill_rect(g, 42, 10, 44, 6, GFX_WHITE);
    gfx_fill_rect(g, 61, 10, 6, 28, GFX_BLACK);  // the ribbon, as a gap
    gfx_fill_rect(g, 42, 15, 44, 1, GFX_BLACK);
    gfx_fill_rect(g, 62, 10, 4, 28, GFX_WHITE);
    gfx_fill_triangle(g, 64, 9, 52, 1, 54, 10, GFX_WHITE);  // the bow
    gfx_fill_triangle(g, 64, 9, 76, 1, 74, 10, GFX_WHITE);
    gfx_line(g, 84, 18, 96, 24, GFX_WHITE);  // the tag on a string
    app_heart(g, 101, 27, 12);
    flake(g, 20, 8, 3);
    flake(g, 30, 28, 2);
    flake(g, 110, 6, 2);
}

static void art_birthday(gfx_t *g) {  // a cake, three candles lit
    gfx_fill_rrect(g, 36, 22, 56, 16, 3, GFX_WHITE);
    gfx_fill_rrect(g, 42, 13, 44, 11, 3, GFX_WHITE);
    for (int x = 38; x < 90; x += 6) gfx_fill_circle(g, x + 2, 22, 2, GFX_BLACK);  // the icing drips
    for (int x = 44; x < 86; x += 7) gfx_pixel(g, x, 17, GFX_BLACK);  // sprinkles
    for (int x = 40; x < 90; x += 9) gfx_pixel(g, x + 3, 31, GFX_BLACK);
    for (int k = 0; k < 3; k++) {
        const int x = 52 + k * 12;
        gfx_fill_rect(g, x, 5, 3, 8, GFX_WHITE);
        gfx_fill_triangle(g, x - 1, 3, x + 3, 3, x + 1, -2, GFX_WHITE);  // flames
        gfx_fill_circle(g, x + 1, 3, 1, GFX_WHITE);
    }
    sparkle(g, 22, 12, 2);
    sparkle(g, 106, 16, 2);
}

static void art_supergirl(gfx_t *g) {  // a hero's shield with an S, cape flying
    gfx_fill_triangle(g, 44, 8, 22, 36, 40, 30, GFX_WHITE);  // the cape
    gfx_fill_triangle(g, 84, 8, 106, 36, 88, 30, GFX_WHITE);
    gfx_fill_triangle(g, 46, 4, 82, 4, 64, 37, GFX_WHITE);  // the shield
    gfx_fill_rect(g, 50, 2, 29, 3, GFX_WHITE);
    font_draw_centered(g, 64, 7, "S", 2, GFX_BLACK);
    sparkle(g, 14, 10, 2);
    sparkle(g, 112, 12, 2);
    sparkle(g, 100, 34, 1);
}

void art_song(gfx_t *g, const char *name) {
    static const struct {
        const char *name;
        void (*draw)(gfx_t *g);
    } ART[] = {
        {"BARBIE GIRL", art_barbie},       {"FINAL COUNTDOWN", art_countdown}, {"TAKE ON ME", art_takeonme},
        {"MORTAL KOMBAT", art_kombat},     {"KOROBEINIKI", art_korobeiniki},   {"HARRY POTTER", art_potter},
        {"IMPERIAL MARCH", art_imperial},  {"JINGLE BELLS", art_jingle},       {"LAST CHRISTMAS", art_christmas},
        {"HAPPY BIRTHDAY", art_birthday},  {"SUPERGIRL", art_supergirl},
    };
    for (unsigned i = 0; i < sizeof(ART) / sizeof(ART[0]); i++) {
        if (strcmp(ART[i].name, name) == 0) {
            ART[i].draw(g);
            return;
        }
    }
    // A song with no picture of its own: a pair of quavers.
    gfx_fill_circle(g, 56, 30, 5, GFX_WHITE);
    gfx_fill_circle(g, 74, 26, 5, GFX_WHITE);
    gfx_fill_rect(g, 60, 8, 2, 22, GFX_WHITE);
    gfx_fill_rect(g, 78, 4, 2, 22, GFX_WHITE);
    for (int r = 0; r < 4; r++) gfx_line(g, 60, 8 + r, 79, 4 + r, GFX_WHITE);
}
