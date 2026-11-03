#include <stdio.h>
#include <string.h>
#include <math.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include "j2vita.h"

/* Adjust mode: one finger drags the game (or the keypad / HUD it lands on),
 * two fingers pinch to size and twist to turn the game. The overlay is
 * 480x272 GL_RGBA, half the screen resolution. */

enum { T_NONE, T_GAME, T_KEYPAD, T_HUD };

static volatile int open_;
static GameConfig before;
static int fingers, target;
static float ax, ay, bx, by;   /* finger positions at the last step, screen pixels */
static float fx, fy, fz, fa;   /* float copies of the target's x/y/scale/angle */

int adjust_is_open(void) { return open_; }

void adjust_open(void) {
    if (!j2v_in_game)
        return;
    before = j2v_cfg;
    fingers = 0;
    target = T_NONE;
    open_ = 1;
    video_redraw();
}

static void finish(int keep) {
    if (keep) {
        if (memcmp(&before, &j2v_cfg, sizeof(before)))
            cfg_save();
    } else {
        j2v_cfg = before;
    }
    open_ = 0;
    hud_changed();
    video_redraw();
}

static int inside(float x, float y, void (*r)(int *, int *, int *, int *)) {
    int rx, ry, rw, rh;
    r(&rx, &ry, &rw, &rh);
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

/* pick what the first finger grabbed and load its numbers into the float copies */
static void grab(float x, float y) {
    int rx, ry, rw, rh;
    if (keypad_is_open() && inside(x, y, video_keypad_rect)) {
        target = T_KEYPAD;
        video_keypad_rect(&rx, &ry, &rw, &rh);
        fx = (float)rx;
        fy = (float)ry;
        fz = (float)j2v_cfg.kp_scale;
    } else if (j2v_cfg.hud && inside(x, y, video_hud_rect)) {
        target = T_HUD;
        video_hud_rect(&rx, &ry, &rw, &rh);
        fx = (float)rx;
        fy = (float)ry;
        fz = (float)j2v_cfg.hud_scale;
    } else {
        target = T_GAME;
        fx = (float)j2v_cfg.off_x;
        fy = (float)j2v_cfg.off_y;
        fz = (float)j2v_cfg.zoom;
    }
    fa = (float)j2v_cfg.rotate;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void store(void) {
    int a = (int)lroundf(fa) % 360;
    if (a < 0)
        a += 360;
    switch (target) {
    case T_KEYPAD:
        j2v_cfg.kp_x = (int)fx;
        j2v_cfg.kp_y = (int)fy;
        j2v_cfg.kp_scale = clampi((int)fz, 50, 400);
        break;
    case T_HUD:
        j2v_cfg.hud_x = (int)fx;
        j2v_cfg.hud_y = (int)fy;
        j2v_cfg.hud_scale = clampi((int)fz, 50, 400);
        hud_changed();
        break;
    case T_GAME:
        j2v_cfg.off_x = clampi((int)fx, -SCREEN_W, SCREEN_W);
        j2v_cfg.off_y = clampi((int)fy, -SCREEN_H, SCREEN_H);
        j2v_cfg.zoom = clampi((int)fz, 25, 400);
        j2v_cfg.rotate = a;
        break;
    }
}

void adjust_input(uint32_t p, const void *touch) {
    const SceTouchData *t = touch;
    int n = t ? (t->reportNum > 2 ? 2 : (int)t->reportNum) : 0;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;

    if (p & SCE_CTRL_CROSS) { finish(1); return; }
    if (p & SCE_CTRL_CIRCLE) { finish(0); return; }
    if (p & (SCE_CTRL_TRIANGLE | SCE_CTRL_SQUARE | SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER)) {
        int a = j2v_cfg.rotate;
        if (p & SCE_CTRL_TRIANGLE) {
            j2v_cfg.zoom = 100;
            j2v_cfg.off_x = j2v_cfg.off_y = 0;
            j2v_cfg.kp_x = j2v_cfg.kp_y = j2v_cfg.hud_x = j2v_cfg.hud_y = -1;
            a = (a + 45) / 90 * 90;
        }
        if (p & SCE_CTRL_SQUARE) a = ((a + 45) / 90 + 1) * 90;
        if (p & SCE_CTRL_LTRIGGER) a -= 5;
        if (p & SCE_CTRL_RTRIGGER) a += 5;
        j2v_cfg.rotate = ((a % 360) + 360) % 360;
        fingers = 0;
        hud_changed();
        video_redraw();
    }

    if (n > 0) {
        x0 = t->report[0].x * (float)SCREEN_W / 1920;
        y0 = t->report[0].y * (float)SCREEN_H / 1088;
    }
    if (n > 1) {
        x1 = t->report[1].x * (float)SCREEN_W / 1920;
        y1 = t->report[1].y * (float)SCREEN_H / 1088;
    }
    if (n != fingers) {
        /* a finger came or went: start measuring again from here */
        if (fingers == 0 && n > 0)
            grab(x0, y0);
        fingers = n;
        ax = x0; ay = y0; bx = x1; by = y1;
        if (n == 0)
            target = T_NONE;
        return;
    }
    if (n == 0 || target == T_NONE)
        return;
    if (n == 1) {
        fx += x0 - ax;
        fy += y0 - ay;
    } else {
        float d0 = hypotf(bx - ax, by - ay), d1 = hypotf(x1 - x0, y1 - y0);
        fx += (x0 + x1 - ax - bx) / 2;
        fy += (y0 + y1 - ay - by) / 2;
        if (d0 > 20 && d1 > 20)
            fz *= d1 / d0;
        if (target == T_GAME)
            fa += (atan2f(y1 - y0, x1 - x0) - atan2f(by - ay, bx - ax)) * 180.0f / (float)M_PI;
    }
    ax = x0; ay = y0; bx = x1; by = y1;
    store();
    video_redraw();
}

/* ---- overlay ---- */

static uint32_t *fb;
static int fw, fh;

static void fill(int x0, int y0, int w, int h, uint32_t c) {
    int x, y;
    for (y = y0 < 0 ? 0 : y0; y < y0 + h && y < fh; y++)
        for (x = x0 < 0 ? 0 : x0; x < x0 + w && x < fw; x++)
            fb[y * fw + x] = c;
}

/* dashed outline of a layer, rect in screen pixels */
static void outline(void (*r)(int *, int *, int *, int *), uint32_t c) {
    int x, y, w, h, i;
    r(&x, &y, &w, &h);
    x /= 2; y /= 2; w /= 2; h /= 2;
    for (i = 0; i < w; i++)
        if ((i / 4) & 1) {
            fill(x + i, y, 1, 1, c);
            fill(x + i, y + h - 1, 1, 1, c);
        }
    for (i = 0; i < h; i++)
        if ((i / 4) & 1) {
            fill(x, y + i, 1, 1, c);
            fill(x + w - 1, y + i, 1, 1, c);
        }
}

static void centered(int y, const char *s, uint32_t c) {
    text_draw(fb, fw, fh, (fw - 8 * (int)strlen(s)) / 2, y, s, c);
}

void adjust_draw(uint32_t *buf, int w, int h) {
    char line[64];
    fb = buf;
    fw = w;
    fh = h;
    memset(fb, 0, (size_t)w * h * 4);
    if (keypad_is_open())
        outline(video_keypad_rect, 0xFF40D0FF);
    if (j2v_cfg.hud)
        outline(video_hud_rect, 0xFF40D0FF);
    fill(0, 0, w, 20, 0xA0000000);
    snprintf(line, sizeof(line), "Adjust  %d deg  %d%%", j2v_cfg.rotate, j2v_cfg.zoom);
    centered(2, line, 0xFFFFFFFF);
    fill(0, h - 20, w, 20, 0xA0000000);
    centered(h - 18, "X Done  O Cancel  [] +90  L/R 5  /\\ Reset", 0xFFE0E0E0);
}
