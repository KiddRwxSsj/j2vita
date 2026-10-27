#include <string.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include "javacall_keypress.h"
#include "javacall_penevent.h"
#include "j2vita.h"

#define DEADZONE 60
#define REPEAT_DELAY 25 /* ticks of 10ms */
#define REPEAT_RATE 6
#define KEYPAD_HOLD 50 /* ticks of 10ms holding SELECT that toggle the keypad */
/* buttons the on-screen keypad takes over while it is shown */
#define KEYPAD_BUTTONS (SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT | \
                        SCE_CTRL_CROSS | SCE_CTRL_CIRCLE | SCE_CTRL_SQUARE | SCE_CTRL_TRIANGLE)

static SceUID thread = -1;
static volatile int running;
static unsigned char held[128];
static int hold_ticks[128];
static int pen_down, pen_x, pen_y;

static const uint32_t btn_bits[BTN_COUNT] = {
    SCE_CTRL_CROSS, SCE_CTRL_CIRCLE, SCE_CTRL_SQUARE, SCE_CTRL_TRIANGLE,
    SCE_CTRL_LTRIGGER, SCE_CTRL_RTRIGGER, SCE_CTRL_START,
};

static int slot(int key) { return (key + 32) & 127; }

static int is_direction(int key) {
    return key == JAVACALL_KEY_UP || key == JAVACALL_KEY_DOWN ||
           key == JAVACALL_KEY_LEFT || key == JAVACALL_KEY_RIGHT ||
           key == JAVACALL_KEY_2 || key == JAVACALL_KEY_4 ||
           key == JAVACALL_KEY_6 || key == JAVACALL_KEY_8;
}

static void release_all(void) {
    int i;
    for (i = 0; i < 128; i++)
        if (held[i]) {
            javanotify_key_event((javacall_key)(i - 32), JAVACALL_KEYRELEASED);
            held[i] = 0;
        }
    if (pen_down) {
        javanotify_pen_event(pen_x, pen_y, JAVACALL_PENRELEASED);
        pen_down = 0;
    }
}

/* *kp_key gets the on-screen keypad key under the finger, if any */
static void poll_touch(int *kp_key) {
    SceTouchData t;
    int x, y, touching;
    touching = sceTouchPeek(SCE_TOUCH_PORT_FRONT, &t, 1) >= 0 && t.reportNum > 0;
    if (touching && keypad_is_open() &&
        keypad_hit(t.report[0].x * 480 / 1920, t.report[0].y * 272 / 1088, kp_key)) {
        touching = 0; /* the keypad took it, the game sees the finger lifted */
    } else if (keypad_is_open()) {
        keypad_release_touch();
    }
    if (!touching || !video_touch_to_game(t.report[0].x, t.report[0].y, &x, &y)) {
        if (pen_down) {
            javanotify_pen_event(pen_x, pen_y, JAVACALL_PENRELEASED);
            pen_down = 0;
        }
        return;
    }
    if (!pen_down)
        javanotify_pen_event(x, y, JAVACALL_PENPRESSED);
    else if (x != pen_x || y != pen_y)
        javanotify_pen_event(x, y, JAVACALL_PENDRAGGED);
    pen_down = 1;
    pen_x = x;
    pen_y = y;
}

static void mark(unsigned char *want, int key) {
    if (key != JAVACALL_KEY_INVALID)
        want[slot(key)] = 1;
}

static void game_keys(const SceCtrlData *pad, unsigned char *want) {
    int up, down, left, right, i;
    const GameConfig *c = &j2v_cfg;

    up = (pad->buttons & SCE_CTRL_UP) || pad->ly < 128 - DEADZONE;
    down = (pad->buttons & SCE_CTRL_DOWN) || pad->ly > 128 + DEADZONE;
    left = (pad->buttons & SCE_CTRL_LEFT) || pad->lx < 128 - DEADZONE;
    right = (pad->buttons & SCE_CTRL_RIGHT) || pad->lx > 128 + DEADZONE;

    /* rotated mode: the Vita is held with its right side up */
    if (j2v_in_game && c->rotate) {
        int u = up, d = down, l = left, r = right;
        up = r; down = l; left = u; right = d;
    }
    if (j2v_in_game && c->numpad_dpad) {
        if (up) mark(want, JAVACALL_KEY_2);
        if (down) mark(want, JAVACALL_KEY_8);
        if (left) mark(want, JAVACALL_KEY_4);
        if (right) mark(want, JAVACALL_KEY_6);
    } else {
        if (up) mark(want, JAVACALL_KEY_UP);
        if (down) mark(want, JAVACALL_KEY_DOWN);
        if (left) mark(want, JAVACALL_KEY_LEFT);
        if (right) mark(want, JAVACALL_KEY_RIGHT);
    }

    for (i = 0; i <= BTN_START; i++)
        if (pad->buttons & btn_bits[i])
            mark(want, c->map[i]);
    if (pad->ry < 128 - DEADZONE) mark(want, c->map[BTN_RS_UP]);
    if (pad->ry > 128 + DEADZONE) mark(want, c->map[BTN_RS_DOWN]);
    if (pad->rx < 128 - DEADZONE) mark(want, c->map[BTN_RS_LEFT]);
    if (pad->rx > 128 + DEADZONE) mark(want, c->map[BTN_RS_RIGHT]);
}

static int input_thread(SceSize args, void *argp) {
    SceCtrlData pad;
    uint32_t last = 0, swallow = 0;
    int idle = 0, was_menu = 0, was_game = 0, was_kp = 0, select_ticks = -1;
    (void)args; (void)argp;

    while (running) {
        unsigned char want[128];
        uint32_t pressed;
        int i, kp_key;

        sceKernelDelayThread(10000);
        memset(&pad, 0, sizeof(pad));
        if (sceCtrlPeekBufferPositive(0, &pad, 1) < 0)
            continue;

        /* sticks count as d-pad inside the menu */
        if (pad.ly < 128 - DEADZONE) pad.buttons |= SCE_CTRL_UP;
        if (pad.ly > 128 + DEADZONE) pad.buttons |= SCE_CTRL_DOWN;
        pressed = pad.buttons & ~last;
        last = pad.buttons;

        if (++idle >= 1000) {
            idle = 0;
            if (j2v_in_game)
                sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
        }

        if (menu_is_open()) {
            menu_input(pressed);
            was_menu = 1;
            select_ticks = -1;
            continue;
        }
        /* buttons still held from the menu, the keypad or the launcher must not
         * reach the game (or the launcher after Exit game) until they are let go */
        if (was_menu || was_game != j2v_in_game || was_kp != keypad_is_open())
            swallow = pad.buttons;
        was_menu = 0;
        was_game = j2v_in_game;
        was_kp = keypad_is_open();
        swallow &= pad.buttons;
        pad.buttons &= ~swallow;
        pressed &= ~swallow;

        /* SELECT: a tap opens the menu, holding it in a game toggles the keypad */
        if (pressed & SCE_CTRL_SELECT)
            select_ticks = 0;
        if (select_ticks >= 0 && (pad.buttons & SCE_CTRL_SELECT)) {
            if (++select_ticks == KEYPAD_HOLD && j2v_in_game) {
                select_ticks = -1;
                release_all();
                keypad_toggle();
                continue;
            }
        } else if (select_ticks >= 0) {
            select_ticks = -1;
            release_all();
            menu_open();
            continue;
        }

        kp_key = JAVACALL_KEY_INVALID;
        if (keypad_is_open() && !j2v_in_game)
            keypad_toggle();
        if (keypad_is_open()) {
            kp_key = keypad_input(pressed, pad.buttons);
            pad.buttons &= ~KEYPAD_BUTTONS;
            pad.lx = pad.ly = 128;
        }
        poll_touch(&kp_key);
        memset(want, 0, sizeof(want));
        game_keys(&pad, want);
        mark(want, kp_key);
        for (i = 0; i < 128; i++) {
            javacall_key key = (javacall_key)(i - 32);
            if (want[i] && !held[i]) {
                javanotify_key_event(key, JAVACALL_KEYPRESSED);
                hold_ticks[i] = 0;
            } else if (!want[i] && held[i]) {
                javanotify_key_event(key, JAVACALL_KEYRELEASED);
            } else if (want[i] && is_direction(key)) {
                if (++hold_ticks[i] >= REPEAT_DELAY) {
                    hold_ticks[i] = REPEAT_DELAY - REPEAT_RATE;
                    javanotify_key_event(key, JAVACALL_KEYREPEATED);
                }
            }
            held[i] = want[i];
        }
    }
    return 0;
}

void input_start(void) {
    if (thread >= 0)
        return;
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    running = 1;
    thread = sceKernelCreateThread("j2v_input", input_thread, 0x10000100, 0x4000, 0, 0, NULL);
    if (thread >= 0)
        sceKernelStartThread(thread, 0, NULL);
}

void input_stop(void) {
    if (thread < 0)
        return;
    running = 0;
    sceKernelWaitThreadEnd(thread, NULL, NULL);
    sceKernelDeleteThread(thread);
    thread = -1;
}
