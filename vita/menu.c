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
enum {
    IT_RESUME, IT_KEYPAD, IT_THEME, IT_SCALE, IT_FILTER, IT_ROTATE, IT_SIZE, IT_KEYSET,
    IT_DPAD, IT_CONTROLS, IT_FPS, IT_EXIT_GAME, IT_QUIT, IT_COUNT
};
enum { GLYPH_NONE, GLYPH_CROSS, GLYPH_CIRCLE, GLYPH_SQUARE };

static const char *labels[IT_COUNT] = {
    "Resume", "On-screen keypad", "Theme", "Screen", "Filter", "Rotate", "Resolution",
    "Phone keys", "D-pad sends", "Controls", "Performance", "Exit game", "Quit J2Vita",
};

static const char *btn_names[BTN_COUNT] = {
    "Cross", "Circle", "Square", "Triangle", "L", "R", "Start",
    "Right stick up", "Right stick down", "Right stick left", "Right stick right",
};

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
static int page, sel, top, ctl_sel, pick_sel, restart_needed;
static GameConfig before;

/* in a game the menu edits that game, in the launcher it edits the global settings */
static GameConfig *target(void) { return j2v_in_game ? &j2v_cfg : &j2v_global; }

static int visible(int it) {
    if (j2v_in_game)
        return it != IT_THEME;
    return it == IT_THEME || it == IT_SCALE || it == IT_FILTER || it == IT_KEYSET || it == IT_DPAD ||
           it == IT_CONTROLS || it == IT_FPS;
}

/* rows of the current list page */
static int page_len(void) {
    int i, n = 0;
    if (page != PAGE_MAIN)
        return BTN_COUNT + 1;
    for (i = 0; i < IT_COUNT; i++)
        n += visible(i);
    return n;
}

static void step_sel(int d) {
    int n = page == PAGE_MAIN ? IT_COUNT : BTN_COUNT + 1, i;
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
    case IT_ROTATE: c->rotate = !c->rotate; break;
    case IT_SIZE: c->size = (c->size + d + j2v_size_count) % j2v_size_count; break;
    case IT_KEYSET: c->keyset = (c->keyset + d + KEYSET_COUNT) % KEYSET_COUNT; break;
    case IT_DPAD: c->numpad_dpad = !c->numpad_dpad; break;
    case IT_FPS: c->show_fps = (c->show_fps + d + OSD_COUNT) % OSD_COUNT; break;
    case IT_THEME:
        j2v_global.theme = (j2v_global.theme + d + THEME_COUNT) % THEME_COUNT;
        j2v_theme_serial++;
        break;
    }
    restart_needed = j2v_in_game && (c->size != before.size || c->keyset != before.keyset);
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
        if (sel == BTN_COUNT) {
            GameConfig d;
            cfg_defaults(&d);
            memcpy(target()->map, d.map, sizeof(d.map));
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
    case IT_CONTROLS: page = PAGE_CONTROLS; sel = 0; top = 0; break;
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
    case IT_ROTATE: return c->rotate ? "On" : "Off";
    case IT_SIZE:
        snprintf(buf, len, "%dx%d", j2v_sizes[c->size].w, j2v_sizes[c->size].h);
        return buf;
    case IT_KEYSET: return keysets[c->keyset];
    case IT_DPAD: return c->numpad_dpad ? "2 4 6 8" : "Arrows";
    case IT_FPS: return c->show_fps == OSD_FULL ? "Full" : c->show_fps == OSD_FPS ? "FPS" : "Off";
    case IT_THEME: return j2v_theme_names[j2v_global.theme];
    case IT_KEYPAD: return keypad_is_open() ? "On" : "Off";
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
            const char *v = i == IT_CONTROLS ? ">" : main_value(i, buf, sizeof(buf));
            row(px, y0 + r * ROW_H, pw, selected, last, next_sel, labels[i], v,
                v && i != IT_CONTROLS);
        } else if (i < BTN_COUNT) {
            row(px, y0 + r * ROW_H, pw, selected, last, next_sel, btn_names[i],
                cfg_key_name(c->map[i]), 1);
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
static void grid(int px, int py, int cw, const char *title, int cursor, int pressed, int mark,
                 int lg, const char *left, int rg, const char *right) {
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
        text_center(x + cw / 2, y + 2 + (i == cursor && pressed ? 1 : 0), cw - 8, pick_names[i],
                    i == cursor ? COL_SEL_TEXT : marked ? COL_ACCENT : COL_TEXT);
    }
}

static void draw_picker(void) {
    int cw = 76, pw = grid_w(cw);
    grid((fw - pw) / 2, (fh - grid_h()) / 2, cw, btn_names[ctl_sel], pick_sel, 0,
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

#define KP_CW 44

static volatile int kp_open;
static int kp_sel = 13, kp_left, kp_down, kp_touch = -1;

static void kp_geometry(int w, int h, int *px, int *py) {
    *px = kp_left ? 6 : w - grid_w(KP_CW) - 6;
    *py = (h - grid_h()) / 2;
}

int keypad_is_open(void) { return kp_open; }

void keypad_toggle(void) {
    kp_open = !kp_open;
    kp_down = 0;
    kp_touch = -1;
    video_redraw();
}

int keypad_input(uint32_t p, uint32_t buttons) {
    int old_sel = kp_sel, old_down = kp_down, old_left = kp_left;
    if (p & SCE_CTRL_CIRCLE) {
        keypad_toggle();
        return JAVACALL_KEY_INVALID;
    }
    if (p & SCE_CTRL_SQUARE)
        kp_left = !kp_left;
    grid_move(&kp_sel, p);
    kp_down = (buttons & SCE_CTRL_CROSS) != 0;
    if (kp_sel != old_sel || kp_down != old_down || kp_left != old_left)
        video_redraw();
    return kp_down ? pick_keys[kp_sel] : JAVACALL_KEY_INVALID;
}

int keypad_hit(int x, int y, int *key) {
    int px, py, gx, gy, col, row;
    kp_geometry(480, 272, &px, &py);
    *key = JAVACALL_KEY_INVALID;
    if (x < px || y < py || x >= px + grid_w(KP_CW) || y >= py + grid_h()) {
        if (kp_touch >= 0) {
            kp_touch = -1;
            video_redraw();
        }
        return 0;
    }
    gx = x - (px + 12);
    gy = y - (py + TITLE_H + 6);
    col = gx / (KP_CW + GRID_GAP);
    row = gy / (GRID_CH + 3);
    if (gx >= 0 && gy >= 0 && col < 3 && row < 7 && gx % (KP_CW + GRID_GAP) < KP_CW) {
        int cell = row * 3 + col;
        if (cell != kp_touch) {
            kp_touch = cell;
            kp_sel = cell;
            video_redraw();
        }
        *key = pick_keys[cell];
    }
    return 1;
}

void keypad_release_touch(void) {
    if (kp_touch >= 0) {
        kp_touch = -1;
        video_redraw();
    }
}

void keypad_draw(uint32_t *buf, int w, int h) {
    int px, py;
    fb = buf;
    fw = w;
    fh = h;
    memset(fb, 0, (size_t)w * h * 4);
    kp_geometry(w, h, &px, &py);
    grid(px, py, KP_CW, "Keypad", kp_sel, kp_down || kp_touch >= 0, JAVACALL_KEY_INVALID,
         GLYPH_CROSS, "Press", GLYPH_CIRCLE, "Hide");
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
        title = restart_needed ? "Applies when the game restarts" : j2v_in_game ? "Options" : "Global settings";
    else
        title = "Controls";
    y0 = panel(px, py, pw, ph, title, restart_needed && page == PAGE_MAIN ? COL_ACCENT : T(TC_BAR_TEXT),
               GLYPH_CROSS, "Select", GLYPH_CIRCLE, page == PAGE_MAIN && !j2v_in_game ? "Close" : "Back");
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
}
