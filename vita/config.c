#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#include "javacall_keypress.h"
#include "j2vita.h"

void javacall_printf(const char *fmt, ...);
void autokeys_set_jar(const char *hint);

GameSize j2v_sizes[] = {
    {240, 320}, {176, 208}, {176, 220}, {128, 160},
    {320, 240}, {240, 400}, {360, 640}, {480, 272}, {640, 360},
    {480, 320}, {320, 480}, {352, 416}, {208, 208}, {128, 128}, {132, 176}, {400, 240},
    {480, 800}, {800, 480},
    {0, 0},
};
const int j2v_size_count = sizeof(j2v_sizes) / sizeof(j2v_sizes[0]);

GameConfig j2v_cfg, j2v_global;
int j2v_in_game;
static char game_file[256];
/* what the current game gets when it has no file of its own: the global
 * settings plus the screen size guessed for it */
static GameConfig game_base;

static const struct { int key; const char *name; } keys[] = {
    {JAVACALL_KEY_INVALID, "None"},
    {JAVACALL_KEY_SELECT, "Fire"},
    {JAVACALL_KEY_SOFT1, "Soft Left"},
    {JAVACALL_KEY_SOFT2, "Soft Right"},
    {JAVACALL_KEY_UP, "Up"}, {JAVACALL_KEY_DOWN, "Down"},
    {JAVACALL_KEY_LEFT, "Left"}, {JAVACALL_KEY_RIGHT, "Right"},
    {JAVACALL_KEY_0, "0"}, {JAVACALL_KEY_1, "1"}, {JAVACALL_KEY_2, "2"},
    {JAVACALL_KEY_3, "3"}, {JAVACALL_KEY_4, "4"}, {JAVACALL_KEY_5, "5"},
    {JAVACALL_KEY_6, "6"}, {JAVACALL_KEY_7, "7"}, {JAVACALL_KEY_8, "8"},
    {JAVACALL_KEY_9, "9"},
    {JAVACALL_KEY_ASTERISK, "*"}, {JAVACALL_KEY_POUND, "#"},
    {JAVACALL_KEY_CLEAR, "Clear"},
};
#define KEY_COUNT ((int)(sizeof(keys) / sizeof(keys[0])))

static const char *btn_ids[BTN_COUNT] = {
    "cross", "circle", "square", "triangle", "l", "r", "start",
    "rs_up", "rs_down", "rs_left", "rs_right",
    "rear_l", "rear_r", "front_l", "front_r",
};

/* the first CFG_INTS ints of GameConfig, in order */
static const struct { const char *name; int lo, hi; } fields[CFG_INTS] = {
    {"scale", 0, SCALE_COUNT - 1}, {"smooth", 0, 1}, {"rotate", 0, 359},
    {"size", 0, 63}, {"keyset", 0, KEYSET_COUNT - 1}, {"numpad_dpad", 0, 1},
    {"show_fps", 0, OSD_COUNT - 1}, {"theme", 0, THEME_COUNT - 1},
    {"zoom", 25, 400}, {"off_x", -960, 960}, {"off_y", -544, 544},
    {"touch", 0, TOUCH_COUNT - 1}, {"hud", 0, 1},
    {"hud_x", -1, 960}, {"hud_y", -1, 544}, {"hud_scale", 50, 400},
    {"kp_x", -1, 960}, {"kp_y", -1, 544}, {"kp_scale", 50, 400}, {"autokeys", 0, 1},
    {"kp_alpha", 20, 100}, {"kp_stay", 0, 1}, {"game_w", 0, GAME_MAX_W}, {"game_h", 0, GAME_MAX_H},
    {"autosize", 0, 1},
};

const char *cfg_key_name(int javakey) {
    int i;
    for (i = 0; i < KEY_COUNT; i++)
        if (keys[i].key == javakey)
            return keys[i].name;
    return "?";
}

int cfg_next_key(int javakey, int dir) {
    int i;
    for (i = 0; i < KEY_COUNT; i++)
        if (keys[i].key == javakey)
            return keys[(i + dir + KEY_COUNT) % KEY_COUNT].key;
    return keys[0].key;
}

void cfg_defaults(GameConfig *c) {
    memset(c, 0, sizeof(*c));
    c->scale = SCALE_FIT;
    c->smooth = 0;
    c->size = 0;
    c->keyset = KEYSET_NOKIA;
    c->zoom = 100;
    c->hud_x = c->hud_y = c->kp_x = c->kp_y = -1;
    c->hud_scale = 100;
    c->kp_scale = 200;
    c->autokeys = 1;
    c->kp_alpha = 85;
    c->autosize = 1;
    c->map[BTN_CROSS] = JAVACALL_KEY_SELECT;
    c->map[BTN_CIRCLE] = JAVACALL_KEY_SOFT2;
    c->map[BTN_SQUARE] = JAVACALL_KEY_ASTERISK;
    c->map[BTN_TRIANGLE] = JAVACALL_KEY_POUND;
    c->map[BTN_L] = JAVACALL_KEY_SOFT1;
    c->map[BTN_R] = JAVACALL_KEY_SOFT2;
    c->map[BTN_START] = JAVACALL_KEY_SOFT1;
    c->map[BTN_RS_UP] = JAVACALL_KEY_2;
    c->map[BTN_RS_DOWN] = JAVACALL_KEY_8;
    c->map[BTN_RS_LEFT] = JAVACALL_KEY_4;
    c->map[BTN_RS_RIGHT] = JAVACALL_KEY_6;
    c->map[BTN_REAR_L] = JAVACALL_KEY_1;
    c->map[BTN_REAR_R] = JAVACALL_KEY_3;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* default.cfg holds every field, so new defaults need a step here to reach existing installs */
#define CFG_VERSION 2
static int file_version;

static int load(const char *path, GameConfig *c) {
    char line[128];
    FILE *f = fopen(path, "r");
    file_version = 0;
    if (!f)
        return 0;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        int v, i;
        if (!eq)
            continue;
        *eq = 0;
        v = atoi(eq + 1);
        if (!strcmp(line, "version")) {
            file_version = v;
            continue;
        }
        /* rotate used to be an on/off switch for a quarter turn */
        if (!strcmp(line, "rotate") && v == 1)
            v = 90;
        for (i = 0; i < CFG_INTS; i++)
            if (!strcmp(line, fields[i].name)) {
                (&c->scale)[i] = clampi(v, fields[i].lo, fields[i].hi);
                break;
            }
        if (i < CFG_INTS)
            continue;
        for (i = 0; i < BTN_COUNT; i++)
            if (!strcmp(line, btn_ids[i]))
                c->map[i] = v;
    }
    if (c->size >= j2v_size_count)
        c->size = 0;
    fclose(f);
    return 1;
}

/* writes only the settings that differ from base (all of them when base is NULL);
 * returns how many were written */
static int write_cfg(const char *path, const GameConfig *c, const GameConfig *base) {
    const int *v = &c->scale, *b = base ? &base->scale : NULL;
    int i, n = 0;
    FILE *f = fopen(path, "w");
    if (!f)
        return 0;
    if (!base)
        fprintf(f, "version=%d\n", CFG_VERSION);
    for (i = 0; i < CFG_INTS; i++)
        if (!b || v[i] != b[i]) {
            fprintf(f, "%s=%d\n", fields[i].name, v[i]);
            n++;
        }
    for (i = 0; i < BTN_COUNT; i++)
        if (!base || c->map[i] != base->map[i]) {
            fprintf(f, "%s=%d\n", btn_ids[i], c->map[i]);
            n++;
        }
    fclose(f);
    return n;
}

/* name -> lowercase alnum only, so "Asphalt 4: Elite" becomes asphalt4elite */
static void file_for(const char *name, char *out, int len) {
    char id[64];
    int n = 0;
    for (; name && *name && n < (int)sizeof(id) - 1; name++)
        if (isalnum((unsigned char)*name))
            id[n++] = (char)tolower((unsigned char)*name);
    id[n] = 0;
    snprintf(out, len, J2V_DATA "/config/%s.cfg", n ? id : "unknown");
}

/* handset models as they show up in JAR names and the JumpDevices attribute (any case):
 * loose ones may follow letters ("NokE61", "FIFA08E61"), strict ones need a separator
 * on both sides */
static const struct { const char *name; int strict, w, h; } models[] = {
    {"e61", 0, 320, 240}, {"e62", 0, 320, 240}, {"e63", 0, 320, 240}, {"e71", 0, 320, 240},
    {"e72", 0, 320, 240}, {"e73", 0, 320, 240}, {"6790", 0, 320, 240},
    {"c3", 1, 320, 240}, {"e5", 1, 320, 240}, {"e500", 1, 320, 240}, {"x2", 1, 320, 240},
    {"ot-808", 1, 320, 240}, {"ot808", 1, 320, 240},
    {"n80", 1, 352, 416}, {"n90", 1, 352, 416}, {"e60", 1, 352, 416}, {"e70", 1, 352, 416},
};

static int model_size(const char *hint) {
    const char *p;
    int i, j;
    for (p = hint; *p; p++) {
        int sep_before = p == hint || !isalnum((unsigned char)p[-1]);
        for (i = 0; i < (int)(sizeof(models) / sizeof(models[0])); i++) {
            size_t n = strlen(models[i].name);
            if (strncasecmp(p, models[i].name, n) || isdigit((unsigned char)p[n]) ||
                (models[i].strict && (!sep_before || isalnum((unsigned char)p[n]))))
                continue;
            for (j = 0; j < j2v_size_count; j++)
                if (j2v_sizes[j].w == models[i].w && j2v_sizes[j].h == models[i].h)
                    return j;
        }
    }
    return -1;
}

/* first NNNxNNN in the hint that we have a preset for, or a known handset */
static int guess_size(const char *hint) {
    const char *p;
    int i;
    if (!hint)
        return -1;
    for (p = hint; *p; p++) {
        int w, h;
        if (!isdigit((unsigned char)*p) || (p > hint && isdigit((unsigned char)p[-1])))
            continue;
        if (sscanf(p, "%dx%d", &w, &h) == 2 || sscanf(p, "%dX%d", &w, &h) == 2 ||
            sscanf(p, "%d,%d", &w, &h) == 2 || sscanf(p, "%d*%d", &w, &h) == 2)
            for (i = 0; i < j2v_size_count; i++)
                if (j2v_sizes[i].w == w && j2v_sizes[i].h == h)
                    return i;
    }
    return model_size(hint);
}

/* games whose manifest and file name say nothing about the screen or keys they want */
static const struct { const char *name; int w, h, keyset; } known[] = {
    {"Rollercoaster Revolution", 480, 320, -1},
    {"Resident Evil - The Missions", 176, 208, 0},
    {"Resident Evil: Degeneration", 240, 320, 0},
};

static int known_game(const char *name) {
    int i;
    for (i = 0; name && i < (int)(sizeof(known) / sizeof(known[0])); i++)
        if (strstr(name, known[i].name))
            return i;
    return -1;
}

static int known_size(int k) {
    int j;
    for (j = 0; k >= 0 && j < j2v_size_count; j++)
        if (j2v_sizes[j].w == known[k].w && j2v_sizes[j].h == known[k].h)
            return j;
    return -1;
}

/* portrait games taller than the Vita screen get a quarter turn in Auto */
static int too_tall(int size) {
    return j2v_sizes[size].h > SCREEN_H && j2v_sizes[size].h > j2v_sizes[size].w;
}

void cfg_select_game(const char *name, const char *hint) {
    cfg_defaults(&j2v_global);
    if (load(J2V_DATA "/config/default.cfg", &j2v_global) && file_version < 2) {
        /* up to 0.9.1 the rear touch halves were off and got saved that way */
        GameConfig d;
        cfg_defaults(&d);
        if (!j2v_global.map[BTN_REAR_L])
            j2v_global.map[BTN_REAR_L] = d.map[BTN_REAR_L];
        if (!j2v_global.map[BTN_REAR_R])
            j2v_global.map[BTN_REAR_R] = d.map[BTN_REAR_R];
    }
    if (name) {
        int k = known_game(name), s = known_size(k);
        autokeys_set_jar(hint);
        if (s < 0)
            s = guess_size(hint);
        if (s < 0)
            s = autokeys_screen_size();
        game_base = j2v_global;
        if (s >= 0)
            game_base.size = s;
        if (game_base.autosize && too_tall(game_base.size))
            game_base.rotate = 270;
        if (k >= 0 && known[k].keyset >= 0)
            game_base.keyset = known[k].keyset;
        else if (j2v_global.autokeys)
            cfg_autodetect(&game_base);
        j2v_cfg = game_base;
        file_for(name, game_file, sizeof(game_file));
        load(game_file, &j2v_cfg);
        j2v_sizes[SIZE_CUSTOM].w = j2v_cfg.game_w;
        j2v_sizes[SIZE_CUSTOM].h = j2v_cfg.game_h;
        if (j2v_cfg.size == SIZE_CUSTOM && !j2v_cfg.game_w)
            j2v_cfg.size = game_base.size;
        /* the base is what the player did not touch, so redo it with the turn and load again */
        if (j2v_cfg.autosize && too_tall(j2v_cfg.size) && !too_tall(game_base.size)) {
            int size = j2v_cfg.size;
            game_base.rotate = 270;
            j2v_cfg = game_base;
            load(game_file, &j2v_cfg);
            j2v_cfg.size = size;
        }
        j2v_in_game = 1;
        javacall_printf("game: %s (%s) size %dx%d keys %d\n", name, hint ? hint : "",
                        j2v_sizes[j2v_cfg.size].w, j2v_sizes[j2v_cfg.size].h, j2v_cfg.keyset);
    } else {
        game_file[0] = 0;
        j2v_in_game = 0;
        /* the launcher ignores user tweaks and wants plain Fire/'*'/'#' */
        cfg_defaults(&j2v_cfg);
        j2v_cfg.map[BTN_CROSS] = JAVACALL_KEY_SELECT;
    }
}

int javacall_lcd_get_screen_width(void);
int javacall_lcd_get_screen_height(void);
void javacall_set_new_screen_size(int w, int h);

/*
 * Called while the game draws: it filled or clipped w x h from the screen corner
 * and that is past the screen we gave it, so the game was made for w x h.
 * Presets switch after a few frames, odd sizes need to show up for longer.
 */
void cfg_screen_hint(int w, int h) {
    static int last_w, last_h, hits;
    int i, preset = -1;
    if (!j2v_in_game || !j2v_cfg.autosize || w < 96 || h < 96 || w > GAME_MAX_W || h > GAME_MAX_H ||
        (w <= javacall_lcd_get_screen_width() && h <= javacall_lcd_get_screen_height()))
        return;
    if (w != last_w || h != last_h) {
        last_w = w;
        last_h = h;
        hits = 0;
    }
    for (i = 0; i < SIZE_CUSTOM; i++)
        if (j2v_sizes[i].w == w && j2v_sizes[i].h == h)
            preset = i;
    if (++hits < (preset >= 0 ? 3 : 30) || (preset < 0 && (w > 2 * h || h > 2 * w)))
        return;
    hits = 0;
    if (j2v_cfg.rotate == game_base.rotate) {
        game_base.rotate = j2v_cfg.rotate = h > SCREEN_H && h > w ? 270 : j2v_global.rotate;
    }
    if (preset >= 0) {
        j2v_cfg.size = preset;
        j2v_cfg.game_w = j2v_cfg.game_h = 0;
    } else {
        j2v_cfg.size = SIZE_CUSTOM;
        j2v_cfg.game_w = j2v_sizes[SIZE_CUSTOM].w = w;
        j2v_cfg.game_h = j2v_sizes[SIZE_CUSTOM].h = h;
    }
    cfg_save();
    javacall_printf("screen: the game draws %dx%d, resizing\n", w, h);
    javacall_set_new_screen_size(w, h);
}

/* a game keeps only what the player changed in it, the rest follows the global settings */
void cfg_save(void) {
    sceIoMkdir(J2V_DATA "/config", 0777);
    if (game_file[0] && !write_cfg(game_file, &j2v_cfg, &game_base))
        sceIoRemove(game_file);
}

void cfg_save_global(void) {
    sceIoMkdir(J2V_DATA "/config", 0777);
    write_cfg(J2V_DATA "/config/default.cfg", &j2v_global, NULL);
}

int cfg_device_index(void) {
    return j2v_cfg.size * KEYSET_COUNT + j2v_cfg.keyset;
}
