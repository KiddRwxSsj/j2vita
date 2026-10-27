#ifndef J2VITA_H
#define J2VITA_H

#include <stdint.h>

#define J2V_DATA "ux0:data/j2vita"
#define SCREEN_W 960
#define SCREEN_H 544
#define GAME_MAX_W 640
#define GAME_MAX_H 640

/* buttons the player can remap, SELECT is reserved for the menu */
enum {
    BTN_CROSS, BTN_CIRCLE, BTN_SQUARE, BTN_TRIANGLE,
    BTN_L, BTN_R, BTN_START,
    BTN_RS_UP, BTN_RS_DOWN, BTN_RS_LEFT, BTN_RS_RIGHT,
    BTN_COUNT
};

enum { SCALE_FIT, SCALE_INTEGER, SCALE_STRETCH, SCALE_COUNT };
enum { KEYSET_NOKIA, KEYSET_SE, KEYSET_MOTOROLA, KEYSET_GENERIC, KEYSET_COUNT };
enum { OSD_OFF, OSD_FPS, OSD_FULL, OSD_COUNT };
enum { THEME_WHITE, THEME_BLACK, THEME_BLUE, THEME_GREEN, THEME_COUNT };

/* theme colour slots, keep in sync with J2Style.java */
enum {
    TC_BG_TOP, TC_BG_BOT, TC_BAR_TOP, TC_BAR_BOT, TC_BAR_LIGHT, TC_BAR_EDGE, TC_BAR_TEXT,
    TC_TEXT, TC_DIM, TC_LINE, TC_SEL_TOP, TC_SEL_BOT, TC_SEL_EDGE, TC_SEL_TEXT,
    TC_PANEL_TOP, TC_PANEL_BOT, TC_PANEL_EDGE, TC_SHADOW, TC_ACCENT, TC_GLYPH, TC_WELL,
    TC_WELL_EDGE, TC_DANGER, TC_OK, TC_DISABLED, TC_COUNT
};

typedef struct {
    /* config.c reads/writes the first eight ints by index, keep their order */
    int scale;
    int smooth;
    int rotate;      /* 0 or 1: portrait game turned 90 degrees */
    int size;        /* index into j2v_sizes */
    int keyset;
    int numpad_dpad; /* d-pad sends 2/4/6/8 instead of arrows */
    int show_fps;    /* OSD_* */
    int theme;       /* THEME_*, only the global value is used */
    int map[BTN_COUNT];
} GameConfig;

typedef struct { int w, h; } GameSize;
extern const GameSize j2v_sizes[];
extern const int j2v_size_count;

extern GameConfig j2v_cfg;     /* what the running game / launcher uses */
extern GameConfig j2v_global;  /* defaults for every game, config/default.cfg */
extern int j2v_in_game;

/* config.c */
void cfg_defaults(GameConfig *c);
void cfg_select_game(const char *name, const char *hint);
void cfg_save(void);
void cfg_save_global(void);
int cfg_device_index(void);
const char *cfg_key_name(int javakey);
int cfg_next_key(int javakey, int dir);

/* theme.c */
extern const char *const j2v_theme_names[THEME_COUNT];
extern int j2v_theme_serial; /* bumped whenever the global theme changes */
uint32_t theme_color(int slot);

/* javacall events.c: microseconds the VM spent waiting for events (idle) */
extern volatile uint64_t j2v_vm_idle_us;

/* video.c */
void video_start(void);
void video_set_game_size(int w, int h);
void video_submit(const uint16_t *src, int y0, int y1, int src_is_rows);
int video_touch_to_game(int tx, int ty, int *gx, int *gy);
void video_wait_unpaused(void);
void video_redraw(void);
void text_draw(uint32_t *fb, int fw, int fh, int x, int y, const char *s, uint32_t color);

/* input.c */
void input_start(void);
void input_stop(void);

/* menu.c, all called from the input thread except menu_draw */
void menu_open(void);
int menu_is_open(void);
void menu_input(uint32_t pressed);
void menu_draw(uint32_t *fb, int w, int h);

/* on-screen phone keypad, also in menu.c */
void keypad_toggle(void);
int keypad_is_open(void);
/* input thread: handles pad edges, returns the key held down (or JAVACALL_KEY_INVALID == 0) */
int keypad_input(uint32_t pressed, uint32_t buttons);
/* touch in overlay pixels (480x272): 1 if it lands on the keypad, *key = key under it or 0 */
int keypad_hit(int x, int y, int *key);
void keypad_release_touch(void);
void keypad_draw(uint32_t *fb, int w, int h);

#endif
