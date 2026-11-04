#ifndef J2VITA_H
#define J2VITA_H

#include <stdint.h>
#include <psp2/kernel/cpu.h>

/* pinning helpers to cores 1 and 2 left them unscheduled on some consoles (black screen) */
#define J2V_HELPER_CPUS 0

#define J2V_DATA "ux0:data/j2vita"
#define SCREEN_W 960
#define SCREEN_H 544
#define GAME_MAX_W 800
#define GAME_MAX_H 800

/* buttons the player can remap, SELECT is reserved for the menu */
enum {
    BTN_CROSS, BTN_CIRCLE, BTN_SQUARE, BTN_TRIANGLE,
    BTN_L, BTN_R, BTN_START,
    BTN_RS_UP, BTN_RS_DOWN, BTN_RS_LEFT, BTN_RS_RIGHT,
    BTN_REAR_L, BTN_REAR_R, BTN_FRONT_L, BTN_FRONT_R,
    BTN_COUNT
};

enum { SCALE_FIT, SCALE_INTEGER, SCALE_STRETCH, SCALE_COUNT };
enum { KEYSET_NOKIA, KEYSET_SE, KEYSET_MOTOROLA, KEYSET_GENERIC, KEYSET_COUNT };
enum { OSD_OFF, OSD_FPS, OSD_FULL, OSD_COUNT };
enum { THEME_WHITE, THEME_BLACK, THEME_BLUE, THEME_GREEN, THEME_COUNT };
enum { TOUCH_POINTER, TOUCH_BUTTONS, TOUCH_COUNT };

/* theme colour slots, keep in sync with J2Style.java */
enum {
    TC_BG_TOP, TC_BG_BOT, TC_BAR_TOP, TC_BAR_BOT, TC_BAR_LIGHT, TC_BAR_EDGE, TC_BAR_TEXT,
    TC_TEXT, TC_DIM, TC_LINE, TC_SEL_TOP, TC_SEL_BOT, TC_SEL_EDGE, TC_SEL_TEXT,
    TC_PANEL_TOP, TC_PANEL_BOT, TC_PANEL_EDGE, TC_SHADOW, TC_ACCENT, TC_GLYPH, TC_WELL,
    TC_WELL_EDGE, TC_DANGER, TC_OK, TC_DISABLED, TC_COUNT
};

typedef struct {
    /* config.c reads/writes the first CFG_INTS ints by index, keep their order */
    int scale;
    int smooth;
    int rotate;      /* clockwise degrees, 0..359 */
    int size;        /* index into j2v_sizes */
    int keyset;
    int numpad_dpad; /* d-pad sends 2/4/6/8 instead of arrows */
    int show_fps;    /* OSD_* */
    int theme;       /* THEME_*, only the global value is used */
    int zoom;        /* percent of the Screen mode size */
    int off_x, off_y; /* screen pixels the game is moved by */
    int touch;       /* TOUCH_*: front screen is the pen or two buttons */
    int hud;         /* show the button HUD */
    int hud_x, hud_y, hud_scale;  /* -1 = default corner; scale in percent */
    int kp_x, kp_y, kp_scale;     /* on-screen keypad, same rules */
    int autokeys;    /* keyset, d-pad and face buttons guessed from the JAR */
    int kp_alpha;    /* keypad opacity in percent */
    int kp_stay;     /* keypad stays on screen for touch while the buttons play the game */
    int game_w, game_h; /* size seen in the game's drawing when no preset matches */
    int autosize;    /* follow the size the game draws at */
    int map[BTN_COUNT];
} GameConfig;
#define CFG_INTS 25

typedef struct { int w, h; } GameSize;
/* the last entry is the current game's own size (game_w x game_h), 0x0 when unused */
extern GameSize j2v_sizes[];
#define SIZE_CUSTOM (j2v_size_count - 1)
extern const int j2v_size_count;

extern GameConfig j2v_cfg;     /* what the running game / launcher uses */
extern GameConfig j2v_global;  /* defaults for every game, config/default.cfg */
extern int j2v_in_game;

/* config.c */
void cfg_defaults(GameConfig *c);
void cfg_select_game(const char *name, const char *hint);
void cfg_autodetect(GameConfig *c);
int autokeys_screen_size(void);
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
extern volatile int j2v_events_ready;

/* video.c */
void video_start(void);
void video_set_game_size(int w, int h);
void video_submit(const uint16_t *src, int y0, int y1, int src_is_rows);
int video_touch_to_game(int tx, int ty, int *gx, int *gy);
/* screen rectangles of the keypad and HUD layers */
void video_keypad_rect(int *x, int *y, int *w, int *h);
void video_hud_rect(int *x, int *y, int *w, int *h);
void hud_changed(void);
void video_wait_unpaused(void);
void video_redraw(void);
void text_draw(uint32_t *fb, int fw, int fh, int x, int y, const char *s, uint32_t color);

/* input.c */
void input_start(void);
void input_stop(void);

/* adjust.c: touch editing of the game screen, keypad and HUD placement */
void adjust_open(void);
int adjust_is_open(void);
void adjust_input(uint32_t pressed, const void *touch); /* SceTouchData* */
void adjust_draw(uint32_t *fb, int w, int h);

/* button HUD: one slim bar drawn by menu.c in the menu look */
#define HUD_W 480
#define HUD_H 23
void hud_draw(uint32_t *fb);

/* menu.c, all called from the input thread except menu_draw */
void menu_open(void);
int menu_is_open(void);
void menu_input(uint32_t pressed);
void menu_draw(uint32_t *fb, int w, int h);
/* performance box in the menu look, render thread only; returns the width it used */
int osd_draw(uint32_t *fb, int w, int h, int fps, int cap, int load, int paced, int full, const char *mode);

/* on-screen phone keypad, also in menu.c */
void keypad_toggle(void);
int keypad_is_open(void);
/* the keypad is open with the buttons driving it (not just shown for touch) */
int keypad_has_buttons(void);
/* input thread: handles pad edges, returns the key held down (or JAVACALL_KEY_INVALID == 0) */
int keypad_input(uint32_t pressed, uint32_t buttons);
/* touch in screen pixels: 1 if it lands on the keypad, *key = key under it or 0 */
int keypad_hit(int x, int y, int *key);
void keypad_release_touch(void);
/* keypad drawn 1:1 into its own KP_W x KP_H layer */
#define KP_W 200
#define KP_H 224
void keypad_draw(uint32_t *fb);

/* ime.c: Vita system keyboard, typed text goes to the game as key presses */
void ime_open(void);
int ime_busy(void);
int ime_poll(void);      /* render thread */
int ime_next_char(void); /* input thread */
/* launcher text field: ask, then poll the state until it leaves BUSY */
enum { IME_ASK_IDLE, IME_ASK_BUSY, IME_ASK_OK, IME_ASK_CANCEL };
void ime_ask(const uint16_t *title, const uint16_t *initial);
int ime_ask_state(void);
const uint16_t *ime_answer(void);

#endif
