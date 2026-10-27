// Renders every expression plus a few animation frames to PBM files, which
// png_sheet.py lays out as one PNG. This is how the face is looked at without
// hardware. It is NOT a substitute for checking the real panel.

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "display/gfx.h"
#include "face/face.h"
#include "face/hud.h"

static void write_pbm(const char *dir, int idx, const char *name, const gfx_t *g) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%02d_%s.pbm", dir, idx, name);
    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror(path);
        return;
    }
    fprintf(fp, "P1\n%d %d\n", GFX_W, GFX_H);
    for (int y = 0; y < GFX_H; y++) {
        for (int x = 0; x < GFX_W; x++) fputc(gfx_get_pixel(g, x, y) ? '1' : '0', fp);
        fputc('\n', fp);
    }
    fclose(fp);
}

static void settle(face_t *f, expression_t e, uint32_t *now) {
    face_set_expression(f, e, *now, 60000);
    for (int i = 0; i < 20; i++) face_update(f, *now += 33);
    f->blinking = false;
    f->next_blink = *now + 100000;
    f->next_idle = *now + 100000;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    static face_t f;
    static gfx_t g;
    uint32_t now = 1000;
    int idx = 0;
    face_init(&f, 1, now);

#define SNAP(name)                     \
    do {                               \
        gfx_clear(&g);                 \
        face_render(&f, &g);           \
        write_pbm(dir, idx++, name, &g); \
    } while (0)

    for (int e = 0; e < EXPR_COUNT; e++) {
        settle(&f, (expression_t)e, &now);
        SNAP(face_expression_name((expression_t)e));
    }

    // Blink frames on neutral: open -> quarter -> shut.
    settle(&f, EXPR_NEUTRAL, &now);
    f.blinking = true;
    f.blink_start = now;
    f.now = now + BLINK_DURATION_MS / 4;
    SNAP("blink_half");
    f.now = now + BLINK_DURATION_MS / 2;
    SNAP("blink_shut");
    f.blinking = false;

    // Glance left, yawn at full.
    f.look_x = -12;
    f.now = now;
    SNAP("glance_left");
    f.look_x = 0;
    settle(&f, EXPR_SLEEPY, &now);
    f.idle = IDLE_YAWN;
    f.idle_start = now - 1300;
    f.now = now;
    SNAP("yawn");
    f.idle = IDLE_NONE;

    // Night, and neutral with a burn-in shift plus a battery icon.
    face_set_night(&f, true, now);
    for (int i = 0; i < 20; i++) face_update(&f, now += 33);
    SNAP("night");
    face_set_night(&f, false, now);
    settle(&f, EXPR_NEUTRAL, &now);
    gfx_clear(&g);
    gfx_set_offset(&g, 2, -2);
    face_render(&f, &g);
    gfx_set_offset(&g, 0, 0);
    const battery_status_t b = {BATTERY_OK, 3800, 55};
    hud_battery(&g, &b, 0);
    write_pbm(dir, idx++, "shifted_with_battery", &g);

    // Petting melted, fury with three puffs, a hang (frozen, then the hint),
    // and the reboot's two screens.
    // The newer animations, one frame each.
    settle(&f, EXPR_NEUTRAL, &now);
    face_boot(&f, now);
    f.now = now + 150;
    SNAP("boot_line");
    f.now = now + 380;
    SNAP("boot_pinch");
    f.now = now + 600;
    SNAP("boot_open");
    f.fx = FX_NONE;
    settle(&f, EXPR_NEUTRAL, &now);
    face_start_idle(&f, IDLE_FLY, now);
    f.now = now + 1200;
    f.look_x = 8;
    SNAP("fly_about");
    f.now = now + 4000;
    f.look_x = 6;
    f.look_y = -3;
    SNAP("fly_landed");
    f.look_x = f.look_y = 0;
    settle(&f, EXPR_HAPPY, &now);
    face_start_idle(&f, IDLE_SNOW, now);
    f.now = now + 3000;
    f.look_y = -3;
    SNAP("snow");
    f.look_y = 0;
    settle(&f, EXPR_NEUTRAL, &now);
    face_start_idle(&f, IDLE_BLOWN, now);
    f.now = now + 600;
    f.look_x = 12;
    SNAP("wind");
    f.look_x = 0;
    f.idle = IDLE_NONE;
    settle(&f, EXPR_LAUGH, &now);
    f.expr_since = now - 300;
    f.now = now;
    SNAP("laugh_tears");
    face_fx(&f, FX_ROLL, NULL, now, 1800);
    f.now = now + 500;
    SNAP("rofl");
    f.fx = FX_NONE;
    settle(&f, EXPR_HAPPY, &now);
    face_talk(&f, 3, now);
    f.now = now + 40;
    SNAP("talking");
    f.talk_until = now;
    face_start_idle(&f, IDLE_STRETCH, now);
    f.now = now + 850;
    SNAP("stretch_tall");
    f.now = now + 1150;
    SNAP("stretch_squat");
    f.idle = IDLE_NONE;

    // Petting: one frame of each stage, then the finale's burst.
    settle(&f, EXPR_BLISS, &now);
    f.petting = true;
    static const struct {
        uint32_t at;
        const char *name;
    } PET[] = {{150, "pet_squish"}, {2000, "pet_nuzzle"}, {4500, "pet_melt"}, {5600, "pet_hearts"}};
    for (int k = 0; k < 4; k++) {
        f.pet_since = f.now - PET[k].at;
        SNAP(PET[k].name);
    }
    f.petting = false;
    face_fx(&f, FX_HEARTS, NULL, f.now, 1800);
    f.now += 700;
    f.expr = EXPR_LOVE;
    SNAP("pet_finale");
    f.fx = FX_NONE;
    settle(&f, EXPR_FURIOUS, &now);
    f.rage = 4;
    SNAP("furious_boiling");
    face_hang(&f, now);
    f.now = now + 1000;
    SNAP("hang");
    f.now = now + 6700;
    SNAP("hang_shake_me");
    face_on_event(&f, INPUT_SHAKE, now);
    face_on_event(&f, INPUT_SHAKE, now);
    face_on_event(&f, INPUT_SHAKE, now);
    f.now = f.fx_since + 1200;
    SNAP("reboot_bios");
    f.now = f.fx_since + 2400;
    SNAP("reboot_ok");
    return 0;
}
