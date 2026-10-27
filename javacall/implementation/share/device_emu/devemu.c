#include "javacall_devemu.h"
#include "device.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

/*
 * Device id = size * KEYSET_COUNT + keyset. The sizes must match j2v_sizes
 * in vita/config.c. Rotation is done by the Vita frontend, so it is always 0.
 */
static const int sizes[][2] = {
    {240, 320}, {176, 208}, {176, 220}, {128, 160},
    {320, 240}, {240, 400}, {360, 640}, {480, 272}, {640, 360},
};
#define SIZE_COUNT ((int)(sizeof(sizes) / sizeof(sizes[0])))

typedef struct {
    const char *name;
    const char *pid;
    int up, down, left, right, select, soft1, soft2, clear;
} KEYSET;

static const KEYSET keysets[] = {
    {"Nokia", "nokia-generic", -1, -2, -3, -4, -5, -6, -7, -8},
    {"Sony Ericsson", "se-generic", -1, -2, -3, -4, -5, -6, -7, -8},
    {"Motorola", "motorola-generic", -1, -6, -2, -5, -20, -21, -22, -9},
    {"Generic", "j2me-generic", -1, -2, -3, -4, -5, 21, 22, -8},
};
#define KEYSET_COUNT ((int)(sizeof(keysets) / sizeof(keysets[0])))
#define DEVICE_COUNT (SIZE_COUNT * KEYSET_COUNT)

static int current_device = -1;
static char names[DEVICE_COUNT][40];

static int valid(int id) { return id >= 0 && id < DEVICE_COUNT; }
static const KEYSET *ks(int id) { return &keysets[id % KEYSET_COUNT]; }

int javacall_devemu_get_devices_number() {
    return DEVICE_COUNT;
}

char* javacall_devemu_get_device_name(int id) {
    if (!valid(id))
        return NULL;
    if (!names[id][0])
        snprintf(names[id], sizeof(names[id]), "%s %dx%d", ks(id)->name,
                 sizes[id / KEYSET_COUNT][0], sizes[id / KEYSET_COUNT][1]);
    return names[id];
}

int javacall_devemu_get_device_width(int id) {
    return valid(id) ? sizes[id / KEYSET_COUNT][0] : 480;
}

int javacall_devemu_get_device_height(int id) {
    return valid(id) ? sizes[id / KEYSET_COUNT][1] : 272;
}

int javacall_devemu_get_current_device() {
    return current_device;
}

void javacall_devemu_set_current_device(int id) {
    current_device = id;
}

int javacall_devemu_get_keycode(int id, javacall_key key) {
    const KEYSET *k;
    if (!valid(id))
        return key;
    k = ks(id);
    switch (key) {
    case JAVACALL_KEY_UP: return k->up;
    case JAVACALL_KEY_DOWN: return k->down;
    case JAVACALL_KEY_LEFT: return k->left;
    case JAVACALL_KEY_RIGHT: return k->right;
    case JAVACALL_KEY_SOFT1: return k->soft1;
    case JAVACALL_KEY_SOFT2: return k->soft2;
    case JAVACALL_KEY_CLEAR: return k->clear;
    case JAVACALL_KEY_SELECT: return k->select;
    default: return key;
    }
}

int javacall_devemu_invalid_keycode(int id, int code) {
    const KEYSET *k;
    if (!valid(id))
        return 0;
    k = ks(id);
    return !(code == k->up || code == k->down || code == k->left ||
             code == k->right || code == k->soft1 || code == k->soft2 ||
             code == k->clear || code == k->select);
}

int javacall_devemu_get_rotation(int id) {
    (void)id;
    return 0;
}

int javacall_devemu_get_displayID(int id) {
    return valid(id) ? id : -1;
}

int javacall_devemu_get_deviceID(int dispid) {
    return valid(dispid) ? dispid : -1;
}

char* javacall_devemu_get_device_pid(int id) {
    return valid(id) ? (char *)ks(id)->pid : NULL;
}
