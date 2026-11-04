#include <stdio.h>
#include <string.h>
#include <psp2/ctrl.h>
#include <SDL_mixer.h>
#include "javacall_lifecycle.h"
#include "javacall_keypress.h"
#include "j2vita.h"

/* Same look as the Java launcher (J2Style.java): title bar, light selection bar,
 * soft key bar with PlayStation glyphs. The overlay is 480x272 GL_RGBA, so pixels
 * are 0xAABBGGRR; C() takes the usual 0xRRGGBB. */
#define C(rgb) (0xFF000000u | (((rgb) & 0xFF) << 16) | ((rgb) & 0xFF00) | (((rgb) >> 16) & 0xFF))

/* colours come from the global theme (theme.c), as 0xRRGGBB */
#define T(slot) theme_color(slot)
#define COL_TEXT     T(TC_TEXT)
#define COL_DIM      T(TC_DIM)
#define COL_SEL_TEXT T(TC_SEL_TEXT)
#define COL_ACCENT   T(TC_ACCENT)
#define COL_LINE     T(TC_LINE)
#define COL_EDGE     T(TC_PANEL_EDGE)

#define TITLE_H 28
#define SOFT_H 24
#define ROW_H 19
#define VIS_ROWS 11
#define CHAR_W 8

enum { PAGE_MAIN, PAGE_CONTROLS, PAGE_PICK };
/* top level rows first, then each submenu's rows in the order of its group row */
enum {
    IT_RESUME, IT_G_DISPLAY, IT_G_CONTROLS, IT_G_KEYPAD, IT_THEME, IT_EXIT_GAME, IT_QUIT,
    IT_SIZE, IT_SCALE, IT_FILTER, IT_ROTATE, IT_ADJUST, IT_FPS,
    IT_CONTROLS, IT_KEYSET, IT_DPAD, IT_TOUCH, IT_HUD, IT_AUTOKEYS,
    IT_KEYPAD, IT_KP_SIZE, IT_KP_ALPHA, IT_COUNT
};
enum { GLYPH_NONE, GLYPH_CROSS, GLYPH_CIRCLE, GLYPH_SQUARE, GLYPH_TRIANGLE };

static const char *labels[IT_COUNT] = {
    "Resume", "Display", "Controls", "On-screen keypad", "Theme", "Exit game", "Quit J2Vita",
    "Resolution", "Screen", "Filter", "Rotate", "Adjust screen", "Performance",
    "Button mapping", "Phone keys", "D-pad sends", "Touch screen", "Button HUD", "Auto-detect keys",
    "Show keypad", "Keypad size", "Keypad opacity",
};

static const char *btn_names[BTN_COUNT] = {
    "Cross", "Circle", "Square", "Triangle", "L", "R", "Start",
    "Right stick up", "Right stick down", "Right stick left", "Right stick right",
    "Rear touch left", "Rear touch right", "Left border touch", "Right border touch",
};

/* rows after the buttons on the Controls page */
#define CTL_DETECT BTN_COUNT
#define CTL_RESET (BTN_COUNT + 1)
#define CTL_ROWS (BTN_COUNT + 2)

/* the key picker is laid out like a phone keypad */
static const int pick_keys[21] = {
    JAVACALL_KEY_SOFT1, JAVACALL_KEY_UP, JAVACALL_KEY_SOFT2,
    JAVACALL_KEY_LEFT, JAVACALL_KEY_SELECT, JAVACALL_KEY_RIGHT,
    JAVACALL_KEY_CLEAR, JAVACALL_KEY_DOWN, JAVACALL_KEY_INVALID,
    JAVACALL_KEY_1, JAVACALL_KEY_2, JAVACALL_KEY_3,
    JAVACALL_KEY_4, JAVACALL_KEY_5, JAVACALL_KEY_6,
    JAVACALL_KEY_7, JAVACALL_KEY_8, JAVACALL_KEY_9,
    JAVACALL_KEY_ASTERISK, JAVACALL_KEY_0, JAVACALL_KEY_POUND,
};
static const char *pick_names[21] = {
    "Soft L", "Up", "Soft R", "Left", "Fire", "Right", "Clear", "Down", "None",
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#",
};

static void grid_move(int *cell, uint32_t p);

static volatile int open_;
static int page, group, sel, top, ctl_sel, pick_sel, restart_needed;
static GameConfig before;

/* in a game the menu edits that game, in the launcher it edits the global settings */
static GameConfig *target(void) { return j2v_in_game ? &j2v_cfg : &j2v_global; }

static int group_of(int it) {
    return it < IT_SIZE ? 0 : it < IT_CONTROLS ? IT_G_DISPLAY : it < IT_KEYPAD ? IT_G_CONTROLS : IT_G_KEYPAD;
}

/* rows that open something instead of changing a value */
static int is_link(int it) {
    return it == IT_G_DISPLAY || it == IT_G_CONTROLS || it == IT_G_KEYPAD || it == IT_CONTROLS || it == IT_ADJUST;
}

static int visible(int it) {
    if (group_of(it) != group)
        return 0;
    if (j2v_in_game)
        return it != IT_THEME && it != IT_AUTOKEYS;
    return it == IT_THEME || it == IT_G_DISPLAY || it == IT_G_CONTROLS || it == IT_SCALE || it == IT_FILTER ||
           it == IT_KEYSET || it == IT_DPAD || it == IT_CONTROLS || it == IT_FPS || it == IT_TOUCH ||
           it == IT_HUD || it == IT_AUTOKEYS;
}

/* rows of the current list page */
static int page_len(void) {
    int i, n = 0;
    if (page != PAGE_MAIN)
        return CTL_ROWS;
    for (i = 0; i < IT_COUNT; i++)
        n += visible(i);
    return n;
}

static void step_sel(int d) {
    int n = page == PAGE_MAIN ? IT_COUNT : CTL_ROWS, i;
    for (i = 0; i < n; i++) {
        sel = (sel + d + n) % n;
        if (page != PAGE_MAIN || visible(sel))
            return;
    }
}

static void pause_audio(int pause) {
    int freq, ch;
    Uint16 fmt;
    if (!Mix_QuerySpec(&freq, &fmt, &ch))
        return;
    if (pause) {
        Mix_Pause(-1);
        Mix_PauseMusic();
    } else {
        Mix_Resume(-1);
        Mix_ResumeMusic();
    }
}

int menu_is_open(void) { return open_; }

void javacall_log_flush(void);

void menu_open(void) {
    if (open_)
        return;
    javacall_log_flush();
    before = *target();
    page = PAGE_MAIN;
    group = 0;
    sel = j2v_in_game ? IT_RESUME : IT_THEME;
    top = 0;
    restart_needed = 0;
    pause_audio(1);
    open_ = 1;
    video_redraw();
}

static void close_menu(void) {
    if (memcmp(&before, target(), sizeof(before))) {
        if (j2v_in_game)
            cfg_save();
        else
            cfg_save_global();
    }
    pause_audio(0);
    open_ = 0;
    hud_changed();
    video_redraw();
}

static void change(int d) {
    GameConfig *c = target();
    if (page == PAGE_CONTROLS) {
        if (sel < BTN_COUNT)
            c->map[sel] = cfg_next_key(c->map[sel], d);
        return;
    }
    switch (sel) {
    case IT_SCALE: c->scale = (c->scale + d + SCALE_COUNT) % SCALE_COUNT; break;
    case IT_FILTER: c->smooth = !c->smooth; break;
    case IT_ROTATE: c->rotate = (((c->rotate + 45) / 90 + d + 4) % 4) * 90; break;
    case IT_TOUCH: c->touch = (c->touch + d + TOUCH_COUNT) % TOUCH_COUNT; break;
    case IT_HUD: c->hud = !c->hud; break;
    case IT_AUTOKEYS: c->autokeys = !c->autokeys; break;
    case IT_KEYPAD: c->kp_stay = !c->kp_stay; break;
    case IT_KP_SIZE: c->kp_scale = c->kp_scale + d * 25 < 50 ? 50 : c->kp_scale + d * 25 > 400 ? 400 : c->kp_scale + d * 25; break;
    case IT_KP_ALPHA: c->kp_alpha = c->kp_alpha + d * 10 < 20 ? 20 : c->kp_alpha + d * 10 > 100 ? 100 : c->kp_alpha + d * 10; break;
    case IT_SIZE: {
        /* Auto sits before the first preset */
        int n = c->autosize ? (d > 0 ? -1 : j2v_size_count) : c->size;
        do
            n = (n + d + j2v_size_count) % j2v_size_count;
        while (!j2v_sizes[n].w);
        if (!c->autosize && (d > 0 ? n <= c->size : n >= c->size)) {
            c->autosize = 1;
        } else {
            c->autosize = 0;
            c->size = n;
        }
        break;
    }
    case IT_KEYSET: c->keyset = (c->keyset + d + KEYSET_COUNT) % KEYSET_COUNT; break;
    case IT_DPAD: c->numpad_dpad = !c->numpad_dpad; break;
    case IT_FPS: c->show_fps = (c->show_fps + d + OSD_COUNT) % OSD_COUNT; break;
    case IT_THEME:
        j2v_global.theme = (j2v_global.theme + d + THEME_COUNT) % THEME_COUNT;
        j2v_theme_serial++;
        break;
    }
    restart_needed = j2v_in_game && (c->size != before.size || c->autosize != before.autosize || c->keyset != before.keyset);
}

static void open_picker(void) {
    int i, key = target()->map[sel];
    ctl_sel = sel;
    pick_sel = 4;
    for (i = 0; i < 21; i++)
        if (pick_keys[i] == key)
            pick_sel = i;
    page = PAGE_PICK;
}

static void activate(void) {
    if (page == PAGE_PICK) {
        target()->map[ctl_sel] = pick_keys[pick_sel];
        page = PAGE_CONTROLS;
        sel = ctl_sel;
        return;
    }
    if (page == PAGE_CONTROLS) {
        if (sel == CTL_RESET) {
            GameConfig d;
            cfg_defaults(&d);
            memcpy(target()->map, d.map, sizeof(d.map));
        } else if (sel == CTL_DETECT) {
            if (j2v_in_game) {
                cfg_autodetect(&j2v_cfg);
                restart_needed = j2v_cfg.keyset != before.keyset;
            }
        } else {
            open_picker();
        }
        return;
    }
    switch (sel) {
    case IT_RESUME: close_menu(); break;
    case IT_KEYPAD:
        close_menu();
        keypad_toggle();
        break;
    case IT_G_DISPLAY:
    case IT_G_CONTROLS:
    case IT_G_KEYPAD:
        group = sel;
        top = 0;
        step_sel(1);
        break;
    case IT_CONTROLS: page = PAGE_CONTROLS; sel = 0; top = 0; break;
    case IT_ADJUST:
        close_menu();
        adjust_open();
        break;
    case IT_EXIT_GAME:
        close_menu();
        javanotify_shutdown_current();
        break;
    case IT_QUIT:
        close_menu();
        javanotify_shutdown();
        break;
    default: change(1); break;
    }
}

static void back(void) {
    if (page == PAGE_PICK) {
        page = PAGE_CONTROLS;
        sel = ctl_sel;
    } else if (page == PAGE_CONTROLS) {
        page = PAGE_MAIN;
        sel = IT_CONTROLS;
        top = 0;
    } else if (group) {
        sel = group;
        group = 0;
        top = 0;
    } else {
        close_menu();
    }
}

void menu_input(uint32_t p) {
    if (page == PAGE_PICK) {
        grid_move(&pick_sel, p);
    } else {
        if (p & SCE_CTRL_UP) step_sel(-1);
        if (p & SCE_CTRL_DOWN) step_sel(1);
        if (p & SCE_CTRL_LEFT) change(-1);
        if (p & SCE_CTRL_RIGHT) change(1);
    }
    if (p & SCE_CTRL_CROSS) activate();
    else if (p & SCE_CTRL_CIRCLE) back();
    else if (p & SCE_CTRL_SELECT) close_menu();
    if (p)
        video_redraw();
}

/* ---- drawing ---- */

static uint32_t *fb;
static int fw, fh;

static uint32_t mix(uint32_t a, uint32_t b, int t, int n) {
    int r, g, bl;
    if (n <= 0)
        return a;
    r = ((a >> 16) & 0xFF) + ((((int)(b >> 16) & 0xFF) - ((int)(a >> 16) & 0xFF)) * t) / n;
    g = ((a >> 8) & 0xFF) + ((((int)(b >> 8) & 0xFF) - ((int)(a >> 8) & 0xFF)) * t) / n;
    bl = (a & 0xFF) + ((((int)b & 0xFF) - ((int)a & 0xFF)) * t) / n;
    return (uint32_t)((r << 16) | (g << 8) | bl);
}

static void hline(int x0, int x1, int y, uint32_t c) {
    int x;
    if (y < 0 || y >= fh)
        return;
    if (x0 < 0) x0 = 0;
    if (x1 >= fw) x1 = fw - 1;
    for (x = x0; x <= x1; x++)
        fb[y * fw + x] = c;
}

static void plot(int x, int y, uint32_t c) {
    if (x >= 0 && y >= 0 && x < fw && y < fh)
        fb[y * fw + x] = c;
}

static void line(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        plot(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (2 * err >= dy) { err += dy; x0 += sx; }
        if (2 * err <= dx) { err += dx; y0 += sy; }
    }
}

/* rows from..to-1 of a rounded w x h box, vertical gradient top->bottom (0xRRGGBB) */
static void round_rows(int x, int y, int w, int h, int r, int from, int to, uint32_t ct, uint32_t cb) {
    int i;
    for (i = from; i < to; i++) {
        int dy = i < r ? r - i : (i > h - 1 - r ? i - (h - 1 - r) : 0), off = 0;
        if (dy > 0) {
            int k = 0;
            while ((k + 1) * (k + 1) <= r * r - dy * dy)
                k++;
            off = r - k;
        }
        hline(x + off, x + w - 1 - off, y + i, C(mix(ct, cb, i - from, to - from - 1)));
    }
}

static void round_box(int x, int y, int w, int h, int r, uint32_t ct, uint32_t cb) {
    round_rows(x, y, w, h, r, 0, h, ct, cb);
}

/* s cut down with "..." to at most max pixels */
static const char *fit(const char *s, int max, char *buf, int len) {
    int n = (int)strlen(s), keep;
    if (n * CHAR_W <= max)
        return s;
    keep = max / CHAR_W - 3;
    if (keep < 0)
        keep = 0;
    if (keep > len - 4)
        keep = len - 4;
    memcpy(buf, s, keep);
    strcpy(buf + keep, "...");
    return buf;
}

static void text(int x, int y, int maxw, const char *s, uint32_t rgb) {
    char buf[64];
    text_draw(fb, fw, fh, x, y, fit(s, maxw, buf, sizeof(buf)), C(rgb));
}

static void text_right(int xr, int y, int maxw, const char *s, uint32_t rgb) {
    char buf[64];
    const char *t = fit(s, maxw, buf, sizeof(buf));
    text_draw(fb, fw, fh, xr - CHAR_W * (int)strlen(t), y, t, C(rgb));
}

static void text_center(int cx, int y, int maxw, const char *s, uint32_t rgb) {
    char buf[64];
    const char *t = fit(s, maxw, buf, sizeof(buf));
    text_draw(fb, fw, fh, cx - CHAR_W * (int)strlen(t) / 2, y, t, C(rgb));
}

static void glyph(int g, int cx, int cy) {
    int x, y;
    uint32_t c = C(T(TC_GLYPH));
    if (g == GLYPH_CROSS) {
        line(cx - 4, cy - 4, cx + 4, cy + 4, c);
        line(cx - 3, cy - 4, cx + 4, cy + 3, c);
        line(cx + 4, cy - 4, cx - 4, cy + 4, c);
        line(cx + 3, cy - 4, cx - 4, cy + 3, c);
    } else if (g == GLYPH_CIRCLE) {
        for (y = -5; y <= 5; y++)
            for (x = -5; x <= 5; x++) {
                int d = x * x + y * y;
                if (d >= 13 && d <= 30)
                    plot(cx + x, cy + y, c);
            }
    } else if (g == GLYPH_SQUARE) {
        for (y = -4; y <= 4; y++)
            for (x = -4; x <= 4; x++)
                if (x <= -3 || x >= 3 || y <= -3 || y >= 3)
                    plot(cx + x, cy + y, c);
    } else if (g == GLYPH_TRIANGLE) {
        line(cx, cy - 5, cx - 5, cy + 4, c);
        line(cx, cy - 5, cx + 5, cy + 4, c);
        line(cx - 5, cy + 4, cx + 5, cy + 4, c);
        line(cx, cy - 4, cx - 4, cy + 4, c);
        line(cx, cy - 4, cx + 4, cy + 4, c);
    }
}

static void selection(int x, int y, int w, int h) {
    round_box(x + 1, y + 2, w, h, 5, T(TC_SHADOW), T(TC_SHADOW));
    round_box(x, y, w, h, 5, T(TC_SEL_EDGE), T(TC_SEL_EDGE));
    round_box(x + 1, y + 1, w - 2, h - 2, 4, T(TC_SEL_TOP), T(TC_SEL_BOT));
}

/* panel with title bar and soft key bar; returns the y of its first content row */
static int panel(int x, int y, int w, int h, const char *title, uint32_t title_rgb,
                 int lg, const char *left, int rg, const char *right) {
    int sy = h - SOFT_H, half = w / 2 - 12;
    round_box(x + 3, y + 4, w, h, 6, T(TC_SHADOW), T(TC_SHADOW));
    round_box(x, y, w, h, 6, COL_EDGE, COL_EDGE);
    round_rows(x + 1, y + 1, w - 2, h - 2, 5, 0, TITLE_H - 1, T(TC_BAR_TOP), T(TC_BAR_BOT));
    round_rows(x + 1, y + 1, w - 2, h - 2, 5, TITLE_H - 1, sy - 1, T(TC_PANEL_TOP), T(TC_PANEL_BOT));
    round_rows(x + 1, y + 1, w - 2, h - 2, 5, sy - 1, h - 2, T(TC_BAR_TOP), T(TC_BAR_BOT));
    hline(x + 1, x + w - 2, y + TITLE_H - 1, C(T(TC_BAR_EDGE)));
    hline(x + 1, x + w - 2, y + sy, C(T(TC_BAR_EDGE)));
    text_center(x + w / 2, y + (TITLE_H - 16) / 2, w - 24, title, title_rgb);
    if (left) {
        glyph(lg, x + 17, y + sy + SOFT_H / 2);
        text(x + 28, y + sy + (SOFT_H - 16) / 2 + 1, half - 28, left, T(TC_BAR_TEXT));
    }
    if (right) {
        char buf[64];
        const char *t = fit(right, half - 28, buf, sizeof(buf));
        int tx = x + w - 12 - CHAR_W * (int)strlen(t);
        glyph(rg, tx - 11, y + sy + SOFT_H / 2);
        text(tx, y + sy + (SOFT_H - 16) / 2 + 1, half, t, T(TC_BAR_TEXT));
    }
    return y + TITLE_H + 4;
}

static const char *main_value(int it, char *buf, int len) {
    static const char *scales[] = {"Fit", "Integer", "Stretch"};
    static const char *keysets[] = {"Nokia", "Sony Ericsson", "Motorola", "Generic"};
    const GameConfig *c = target();
    switch (it) {
    case IT_SCALE: return scales[c->scale];
    case IT_FILTER: return c->smooth ? "Smooth" : "Sharp";
    case IT_ROTATE:
        snprintf(buf, len, "%d deg", c->rotate);
        return buf;
    case IT_TOUCH: return c->touch == TOUCH_BUTTONS ? "Buttons" : "Pointer";
    case IT_HUD: return c->hud ? "On" : "Off";
    case IT_AUTOKEYS: return c->autokeys ? "On" : "Off";
    case IT_SIZE:
        snprintf(buf, len, c->autosize ? "Auto %dx%d" : "%dx%d", j2v_sizes[c->size].w, j2v_sizes[c->size].h);
        return buf;
    case IT_KEYSET: return keysets[c->keyset];
    case IT_DPAD: return c->numpad_dpad ? "2 4 6 8" : "Arrows";
    case IT_FPS: return c->show_fps == OSD_FULL ? "Full" : c->show_fps == OSD_FPS ? "FPS" : "Off";
    case IT_THEME: return j2v_theme_names[j2v_global.theme];
    case IT_KEYPAD: return c->kp_stay ? "Always" : keypad_has_buttons() ? "On" : "Off";
    case IT_KP_SIZE:
        snprintf(buf, len, "%d%%", c->kp_scale);
        return buf;
    case IT_KP_ALPHA:
        snprintf(buf, len, "%d%%", c->kp_alpha);
        return buf;
    default: return NULL;
    }
}

/* one list row; arrows mark values that Left/Right can change */
static void row(int x, int y, int w, int selected, int last, int next_sel,
                const char *label, const char *value, int arrows) {
    char vbuf[48];
    int vw = 0, inner = w - 28;
    uint32_t tc = selected ? COL_SEL_TEXT : COL_TEXT;
    if (selected)
        selection(x + 5, y, w - 10, ROW_H);
    else if (!last && !next_sel)
        hline(x + 12, x + w - 13, y + ROW_H - 1, C(COL_LINE));
    if (value) {
        if (arrows && selected)
            snprintf(vbuf, sizeof(vbuf), "< %s >", value);
        else
            snprintf(vbuf, sizeof(vbuf), "%s", value);
        vw = CHAR_W * (int)strlen(vbuf);
        if (vw > inner / 2 + 40)
            vw = inner / 2 + 40;
        text_right(x + w - 14, y + 2, vw, vbuf, selected ? COL_SEL_TEXT : COL_ACCENT);
        vw += 12;
    }
    text(x + 14, y + 2, inner - vw, label, tc);
}

static void draw_list(int px, int pw, int y0) {
    char buf[32];
    int n = page_len(), i, r = 0, idx = 0, cur = 0;
    const GameConfig *c = target();

    /* position of sel among the shown rows, to scroll it into view */
    if (page == PAGE_MAIN) {
        for (i = 0; i < sel; i++)
            cur += visible(i);
    } else {
        cur = sel;
    }
    if (cur < top) top = cur;
    if (cur >= top + VIS_ROWS) top = cur - VIS_ROWS + 1;

    for (i = 0; i < (page == PAGE_MAIN ? IT_COUNT : n); i++) {
        int last, next_sel, selected;
        if (page == PAGE_MAIN && !visible(i))
            continue;
        if (idx < top || idx >= top + VIS_ROWS) {
            idx++;
            continue;
        }
        selected = i == sel;
        last = idx == n - 1 || r == VIS_ROWS - 1;
        next_sel = idx + 1 == cur;
        if (page == PAGE_MAIN) {
            const char *v = is_link(i) ? ">" : main_value(i, buf, sizeof(buf));
            row(px, y0 + r * ROW_H, pw, selected, last, next_sel, labels[i], v, v && !is_link(i));
        } else if (i < BTN_COUNT) {
            row(px, y0 + r * ROW_H, pw, selected, last, next_sel, btn_names[i],
                cfg_key_name(c->map[i]), 1);
        } else if (i == CTL_DETECT) {
            row(px, y0 + r * ROW_H, pw, selected, last, next_sel,
                j2v_in_game ? "Detect from game" : "Detect from game (in game)", NULL, 0);
        } else {
            row(px, y0 + r * ROW_H, pw, selected, 1, 0, "Reset to defaults", NULL, 0);
        }
        idx++;
        r++;
    }
    if (n > VIS_ROWS) {
        int track = VIS_ROWS * ROW_H - 4, thumb = track * VIS_ROWS / n;
        int pos = y0 + 2 + (track - thumb) * top / (n - VIS_ROWS), j;
        for (j = 0; j < track; j++)
            hline(px + pw - 7, px + pw - 5, y0 + 2 + j, C(T(TC_WELL)));
        for (j = 0; j < thumb; j++)
            hline(px + pw - 7, px + pw - 5, pos + j, C(T(TC_SEL_BOT)));
    }
}

#define GRID_CH 20
#define GRID_GAP 4

static int grid_w(int cw) { return 3 * cw + 2 * GRID_GAP + 24; }
static int grid_h(void) { return TITLE_H + 6 + 7 * GRID_CH + 6 * 3 + 8 + SOFT_H; }

/* the phone keypad grid shared by the key picker and the on-screen keypad;
 * cursor is the focused cell, mark the key drawn with an accent outline */
static void grid(int px, int py, int cw, const char *const *names, const char *title, int cursor,
                 int pressed, int mark, int lg, const char *left, int rg, const char *right) {
    int i, gy = panel(px, py, grid_w(cw), grid_h(), title, T(TC_BAR_TEXT), lg, left, rg, right) + 2;
    for (i = 0; i < 21; i++) {
        int x = px + 12 + (i % 3) * (cw + GRID_GAP), y = gy + (i / 3) * (GRID_CH + 3);
        int marked = mark != JAVACALL_KEY_INVALID && pick_keys[i] == mark;
        if (i == cursor) {
            selection(x, y + (pressed ? 1 : 0), cw, GRID_CH);
        } else {
            uint32_t edge = marked ? COL_ACCENT : T(TC_WELL_EDGE);
            round_box(x, y, cw, GRID_CH, 4, edge, edge);
            round_box(x + 1, y + 1, cw - 2, GRID_CH - 2, 3, T(TC_PANEL_TOP), T(TC_WELL));
        }
        text_center(x + cw / 2, y + 2 + (i == cursor && pressed ? 1 : 0), cw - 8, names[i],
                    i == cursor ? COL_SEL_TEXT : marked ? COL_ACCENT : COL_TEXT);
    }
}

static void draw_picker(void) {
    int cw = 76, pw = grid_w(cw);
    grid((fw - pw) / 2, (fh - grid_h()) / 2, cw, pick_names, btn_names[ctl_sel], pick_sel, 0,
         target()->map[ctl_sel], GLYPH_CROSS, "Set", GLYPH_CIRCLE, "Back");
}

static void grid_move(int *cell, uint32_t p) {
    int col = *cell % 3, row = *cell / 3;
    if (p & SCE_CTRL_UP) row = (row + 6) % 7;
    if (p & SCE_CTRL_DOWN) row = (row + 1) % 7;
    if (p & SCE_CTRL_LEFT) col = (col + 2) % 3;
    if (p & SCE_CTRL_RIGHT) col = (col + 1) % 3;
    *cell = row * 3 + col;
}

/* ---- on-screen keypad: sends phone keys to the running game (cheat codes) ---- */

#define KP_CW 56
#define KP_TEXT 8 /* the picker's "None" cell opens the Vita keyboard here */

static const char *kp_names[21] = {
    "Soft L", "Up", "Soft R", "Left", "Fire", "Right", "Clear", "Down", "Text",
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#",
};

static volatile int kp_open;
static int kp_sel = 13, kp_down, kp_touch = -1;
/* dragging the keypad by its title bar: finger offset from the top left corner */
static int kp_drag, kp_dx, kp_dy;

int keypad_is_open(void) { return kp_open || (j2v_in_game && j2v_cfg.kp_stay); }
int keypad_has_buttons(void) { return kp_open; }

void keypad_toggle(void) {
    kp_open = !kp_open;
    kp_down = 0;
    kp_touch = -1;
    kp_drag = 0;
    video_redraw();
}

int keypad_input(uint32_t p, uint32_t buttons) {
    int old_sel = kp_sel, old_down = kp_down;
    if (p & SCE_CTRL_CIRCLE) {
        keypad_toggle();
        return JAVACALL_KEY_INVALID;
    }
    if (p & SCE_CTRL_SQUARE) {
        /* flip to the mirror spot on the other side */
        int x, y, w, h;
        video_keypad_rect(&x, &y, &w, &h);
        j2v_cfg.kp_x = SCREEN_W - x - w;
        j2v_cfg.kp_y = y;
        cfg_save();
        video_redraw();
    }
    grid_move(&kp_sel, p);
    if ((p & SCE_CTRL_CROSS) && kp_sel == KP_TEXT)
        ime_open();
    kp_down = (buttons & SCE_CTRL_CROSS) != 0;
    if (kp_sel != old_sel || kp_down != old_down)
        video_redraw();
    return kp_down ? pick_keys[kp_sel] : JAVACALL_KEY_INVALID;
}

int keypad_hit(int x, int y, int *key) {
    int rx, ry, rw, rh, lx, ly, gx, gy, col, row;
    video_keypad_rect(&rx, &ry, &rw, &rh);
    *key = JAVACALL_KEY_INVALID;
    if (kp_drag) {
        j2v_cfg.kp_x = x - kp_dx;
        j2v_cfg.kp_y = y - kp_dy;
        video_redraw();
        return 1;
    }
    if (x < rx || y < ry || x >= rx + rw || y >= ry + rh) {
        if (kp_touch >= 0) {
            kp_touch = -1;
            video_redraw();
        }
        return 0;
    }
    lx = (x - rx) * KP_W / rw;
    ly = (y - ry) * KP_H / rh;
    if (ly < TITLE_H && kp_touch < 0) {
        kp_drag = 1;
        kp_dx = x - rx;
        kp_dy = y - ry;
        return 1;
    }
    gx = lx - 12;
    gy = ly - (TITLE_H + 6);
    col = gx / (KP_CW + GRID_GAP);
    row = gy / (GRID_CH + 3);
    if (gx >= 0 && gy >= 0 && col < 3 && row < 7 && gx % (KP_CW + GRID_GAP) < KP_CW) {
        int cell = row * 3 + col;
        if (cell != kp_touch) {
            if (cell == KP_TEXT)
                ime_open();
            kp_touch = cell;
            kp_sel = cell;
            video_redraw();
        }
        *key = pick_keys[cell];
    }
    return 1;
}

void keypad_release_touch(void) {
    if (kp_drag) {
        kp_drag = 0;
        cfg_save();
    }
    if (kp_touch >= 0) {
        kp_touch = -1;
        video_redraw();
    }
}

void keypad_draw(uint32_t *buf) {
    fb = buf;
    fw = KP_W;
    fh = KP_H;
    memset(fb, 0, (size_t)KP_W * KP_H * 4);
    if (kp_open)
        grid(0, 0, KP_CW, kp_names, "Keypad", kp_sel, kp_down || kp_touch >= 0, JAVACALL_KEY_INVALID,
             GLYPH_CROSS, "Press", GLYPH_CIRCLE, "Hide");
    else
        grid(0, 0, KP_CW, kp_names, "Keypad", kp_touch, 1, JAVACALL_KEY_INVALID,
             GLYPH_NONE, NULL, GLYPH_NONE, NULL);
}

static const char *key_short(int key) {
    static char d[2];
    if (key >= JAVACALL_KEY_0 && key <= JAVACALL_KEY_9) {
        d[0] = (char)key;
        d[1] = 0;
        return d;
    }
    switch (key) {
    case JAVACALL_KEY_SOFT1: return "SL";
    case JAVACALL_KEY_SOFT2: return "SR";
    case JAVACALL_KEY_SELECT: return "OK";
    case JAVACALL_KEY_ASTERISK: return "*";
    case JAVACALL_KEY_POUND: return "#";
    case JAVACALL_KEY_UP: return "Up";
    case JAVACALL_KEY_DOWN: return "Dn";
    case JAVACALL_KEY_LEFT: return "Lt";
    case JAVACALL_KEY_RIGHT: return "Rt";
    case JAVACALL_KEY_CLEAR: return "C";
    default: return "";
    }
}

/* button -> phone key reminder, right aligned so the default corner anchors it */
void hud_draw(uint32_t *buf) {
    /* face buttons get their glyph, the rest a short label: L/R, rear touch halves, border touch */
    static const int btn[10] = {BTN_CROSS, BTN_CIRCLE, BTN_SQUARE, BTN_TRIANGLE, BTN_L, BTN_R,
                                BTN_REAR_L, BTN_REAR_R, BTN_FRONT_L, BTN_FRONT_R};
    static const int gl[4] = {GLYPH_CROSS, GLYPH_CIRCLE, GLYPH_SQUARE, GLYPH_TRIANGLE};
    static const char *const label[10] = {"", "", "", "", "L", "R", "RL", "RR", "FL", "FR"};
    int i, n = j2v_cfg.touch == TOUCH_BUTTONS ? 10 : 8, w = 0, x, x0;

    fb = buf;
    fw = HUD_W;
    fh = HUD_H;
    memset(fb, 0, (size_t)HUD_W * HUD_H * 4);
    for (i = 0; i < n; i++) {
        int k = (int)strlen(key_short(j2v_cfg.map[btn[i]]));
        if (k)
            w += (i < 4 ? 14 : CHAR_W * (int)strlen(label[i]) + 6) + CHAR_W * k + 12;
    }
    if (!w)
        return;
    w += 12;
    if (w > HUD_W - 3)
        w = HUD_W - 3;
    x0 = HUD_W - w - 3;
    round_box(x0 + 2, 3, w, 20, 10, T(TC_SHADOW), T(TC_SHADOW));
    round_box(x0, 0, w, 20, 10, COL_EDGE, COL_EDGE);
    round_box(x0 + 1, 1, w - 2, 18, 9, T(TC_PANEL_TOP), T(TC_PANEL_BOT));
    x = x0 + 12;
    for (i = 0; i < n; i++) {
        const char *k = key_short(j2v_cfg.map[btn[i]]);
        if (!*k)
            continue;
        if (i < 4) {
            glyph(gl[i], x + 4, 10);
            x += 14;
        } else {
            text(x, 2, CHAR_W * 2, label[i], T(TC_GLYPH));
            x += CHAR_W * (int)strlen(label[i]) + 6;
        }
        text(x, 2, CHAR_W * 2, k, COL_TEXT);
        x += CHAR_W * (int)strlen(k) + 12;
    }
}

int osd_draw(uint32_t *buf, int w, int h, int fps, int cap, int load, int paced, int full, const char *mode) {
    char top[32], pct[8];
    int x = 1, y = 1, bw, bh, bx, bl, fill;

    fb = buf;
    fw = w;
    fh = h;
    memset(fb, 0, (size_t)w * h * 4);
    if (!paced && cap > fps)
        snprintf(top, sizeof(top), "%d/%d FPS", fps, cap);
    else
        snprintf(top, sizeof(top), "%d FPS", fps);
    if (!full) {
        bw = CHAR_W * (int)strlen(top) + 16;
        bh = 22;
        round_box(x + 1, y + 2, bw, bh, 6, T(TC_SHADOW), T(TC_SHADOW));
        round_box(x, y, bw, bh, 6, T(TC_BAR_EDGE), T(TC_BAR_EDGE));
        round_box(x + 1, y + 1, bw - 2, bh - 2, 5, T(TC_BAR_TOP), T(TC_BAR_BOT));
        text_center(x + bw / 2, y + 3, bw, top, T(TC_BAR_TEXT));
        return bw + 3;
    }
    bw = w - 4;
    bh = 22 + 3 * 18 + 6;
    round_box(x + 2, y + 3, bw, bh, 6, T(TC_SHADOW), T(TC_SHADOW));
    round_box(x, y, bw, bh, 6, COL_EDGE, COL_EDGE);
    round_rows(x + 1, y + 1, bw - 2, bh - 2, 5, 0, 21, T(TC_BAR_TOP), T(TC_BAR_BOT));
    round_rows(x + 1, y + 1, bw - 2, bh - 2, 5, 21, bh - 2, T(TC_PANEL_TOP), T(TC_PANEL_BOT));
    hline(x + 1, x + bw - 2, y + 21, C(T(TC_BAR_EDGE)));
    text_center(x + bw / 2, y + 3, bw - 8, top, T(TC_BAR_TEXT));

    y += 26;
    text(x + 8, y, 32, "CPU", COL_DIM);
    bx = x + 42;
    bl = bw - 42 - 44;
    round_box(bx, y + 4, bl, 9, 4, T(TC_WELL_EDGE), T(TC_WELL_EDGE));
    round_box(bx + 1, y + 5, bl - 2, 7, 3, T(TC_WELL), T(TC_WELL));
    fill = (bl - 2) * (load < 0 ? 0 : load > 100 ? 100 : load) / 100;
    if (fill > 0)
        round_box(bx + 1, y + 5, fill, 7, fill < 7 ? fill / 2 : 3, T(TC_ACCENT), T(TC_ACCENT));
    snprintf(pct, sizeof(pct), "%d%%", load);
    text_right(x + bw - 8, y, 40, pct, COL_TEXT);
    /* paced: the game waits on its own timer, more CPU would not add frames */
    text(x + 8, y + 18, bw - 16, paced ? "Game limit" : "CPU limit", COL_TEXT);
    text(x + 8, y + 36, bw - 16, mode, COL_DIM);
    return bw + 4;
}

void menu_draw(uint32_t *buf, int w, int h) {
    int i, rows, pw = 340, ph, px, py, y0;
    const char *title;

    fb = buf;
    fw = w;
    fh = h;
    for (i = 0; i < w * h; i++)
        fb[i] = 0xB4000000;

    rows = page_len();
    if (rows > VIS_ROWS)
        rows = VIS_ROWS;
    ph = TITLE_H + 4 + rows * ROW_H + 4 + SOFT_H;
    px = (w - pw) / 2;
    py = (h - ph) / 2;
    if (page == PAGE_MAIN)
        title = restart_needed ? "Applies when the game restarts" : group ? labels[group]
                : j2v_in_game ? "Options" : "Global settings";
    else
        title = "Controls";
    y0 = panel(px, py, pw, ph, title, restart_needed && page == PAGE_MAIN ? COL_ACCENT : T(TC_BAR_TEXT),
               GLYPH_CROSS, "Select", GLYPH_CIRCLE, page == PAGE_MAIN && !group && !j2v_in_game ? "Close" : "Back");
    if (page == PAGE_PICK) {
        int save = sel;
        page = PAGE_CONTROLS;
        sel = ctl_sel;
        draw_list(px, pw, y0);
        page = PAGE_PICK;
        sel = save;
        /* push the list back so the picker stands out */
        for (i = 0; i < w * h; i++)
            fb[i] = (fb[i] & 0xFF000000) | ((fb[i] >> 1) & 0x7F7F7F);
        draw_picker();
    } else {
        draw_list(px, pw, y0);
    }
    /* see through a little: 80% in game, 95% over the launcher */
    for (i = 0; i < w * h; i++)
        if ((fb[i] >> 24) == 0xFF)
            fb[i] = (fb[i] & 0xFFFFFF) | (j2v_in_game ? 0xCC000000u : 0xF2000000u);
}
