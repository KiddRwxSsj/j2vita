#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#include "javacall_keypress.h"
#include "j2vita.h"

void javacall_printf(const char *fmt, ...);

const GameSize j2v_sizes[] = {
    {240, 320}, {176, 208}, {176, 220}, {128, 160},
    {320, 240}, {240, 400}, {360, 640}, {480, 272}, {640, 360},
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
    c->map[BTN_CROSS] = JAVACALL_KEY_5;
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
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void load(const char *path, GameConfig *c) {
    char line[128];
    FILE *f = fopen(path, "r");
    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        int v, i;
        if (!eq)
            continue;
        *eq = 0;
        v = atoi(eq + 1);
        if (!strcmp(line, "scale")) c->scale = clampi(v, 0, SCALE_COUNT - 1);
        else if (!strcmp(line, "smooth")) c->smooth = !!v;
        else if (!strcmp(line, "rotate")) c->rotate = !!v;
        else if (!strcmp(line, "size")) c->size = clampi(v, 0, j2v_size_count - 1);
        else if (!strcmp(line, "keyset")) c->keyset = clampi(v, 0, KEYSET_COUNT - 1);
        else if (!strcmp(line, "numpad_dpad")) c->numpad_dpad = !!v;
        else if (!strcmp(line, "show_fps")) c->show_fps = clampi(v, 0, OSD_COUNT - 1);
        else if (!strcmp(line, "theme")) c->theme = clampi(v, 0, THEME_COUNT - 1);
        else
            for (i = 0; i < BTN_COUNT; i++)
                if (!strcmp(line, btn_ids[i]))
                    c->map[i] = v;
    }
    fclose(f);
}

/* writes only the settings that differ from base (all of them when base is NULL);
 * returns how many were written */
static int write_cfg(const char *path, const GameConfig *c, const GameConfig *base) {
    const int *v = &c->scale, *b = base ? &base->scale : NULL;
    static const char *names[] = {"scale", "smooth", "rotate", "size", "keyset", "numpad_dpad", "show_fps", "theme"};
    int i, n = 0;
    FILE *f = fopen(path, "w");
    if (!f)
        return 0;
    for (i = 0; i < 8; i++)
        if (!b || v[i] != b[i]) {
            fprintf(f, "%s=%d\n", names[i], v[i]);
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

/* first NNNxNNN in the hint that we have a preset for, or a known 320x240 handset */
static int guess_size(const char *hint) {
    static const char *qvga_land[] = {"E71", "E72", "E63", "C3_00", "C3-00", "X2_01", "X2-01", NULL};
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
    for (i = 0; qvga_land[i]; i++)
        if (strstr(hint, qvga_land[i]))
            return 4;
    return -1;
}

void cfg_select_game(const char *name, const char *hint) {
    cfg_defaults(&j2v_global);
    load(J2V_DATA "/config/default.cfg", &j2v_global);
    if (name) {
        int s = guess_size(hint);
        game_base = j2v_global;
        if (s >= 0)
            game_base.size = s;
        j2v_cfg = game_base;
        file_for(name, game_file, sizeof(game_file));
        load(game_file, &j2v_cfg);
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
