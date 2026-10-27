#include <stdio.h>
#include <string.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/io/dirent.h>
#include <psp2/power.h>
#include "javacall_lifecycle.h"
#include "javacall_logging.h"
#include "j2vita.h"

#ifndef J2V_VERSION
#define J2V_VERSION "dev"
#endif

/* Java heap (32 MB) + timidity patches + VM tables. vitaGL uses its own memblocks. */
int _newlib_heap_size_user = 128 * 1024 * 1024;

extern void JavaTask(void);
void javacall_log_open(void);
void javacall_log_flush(void);
void javacall_log_batch(void);
int suspend_key_input;

static int exists(const char *p) {
    SceIoStat st;
    return sceIoGetstat(p, &st) >= 0;
}

static void copy_file(const char *src, const char *dst, int overwrite) {
    static char buf[32768];
    SceUID in, out;
    int n;
    if (!overwrite && exists(dst))
        return;
    if ((in = sceIoOpen(src, SCE_O_RDONLY, 0)) < 0)
        return;
    if ((out = sceIoOpen(dst, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777)) >= 0) {
        while ((n = sceIoRead(in, buf, sizeof(buf))) > 0)
            sceIoWrite(out, buf, n);
        sceIoClose(out);
    }
    sceIoClose(in);
}

static void copy_tree(const char *src, const char *dst, int overwrite) {
    SceIoDirent e;
    SceUID d = sceIoDopen(src);
    if (d < 0)
        return;
    sceIoMkdir(dst, 0777);
    while (sceIoDread(d, &e) > 0) {
        char s[512], t[512];
        if (e.d_name[0] == '.')
            continue;
        snprintf(s, sizeof(s), "%s/%s", src, e.d_name);
        snprintf(t, sizeof(t), "%s/%s", dst, e.d_name);
        if (SCE_S_ISDIR(e.d_stat.st_mode))
            copy_tree(s, t, overwrite);
        else
            copy_file(s, t, overwrite);
    }
    sceIoDclose(d);
}

/* the VM wants lib/ and appdb/ next to each other on a writable drive */
static void seed_data(void) {
    static const char *dirs[] = {
        "", "/config", "/games", "/private", "/music", "/photo", "/video",
        "/recordings", "/tones", "/graphics",
    };
    char stamp[64] = "";
    int i, fresh;
    SceUID f;

    for (i = 0; i < (int)(sizeof(dirs) / sizeof(dirs[0])); i++) {
        char p[128];
        snprintf(p, sizeof(p), J2V_DATA "%s", dirs[i]);
        sceIoMkdir(p, 0777);
    }
    if ((f = sceIoOpen(J2V_DATA "/version", SCE_O_RDONLY, 0)) >= 0) {
        int n = sceIoRead(f, stamp, sizeof(stamp) - 1);
        stamp[n > 0 ? n : 0] = 0;
        sceIoClose(f);
    }
    fresh = strcmp(stamp, J2V_VERSION) != 0;

    /* system files follow the VPK, installed games (appdb) and settings are kept */
    copy_tree("app0:/runtime/lib", J2V_DATA "/lib", fresh);
    copy_tree("app0:/runtime/appdb", J2V_DATA "/appdb", 0);
    copy_file("app0:/runtime/j2vita.ini", J2V_DATA "/j2vita.ini", fresh);

    if (fresh && (f = sceIoOpen(J2V_DATA "/version", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777)) >= 0) {
        sceIoWrite(f, J2V_VERSION, strlen(J2V_VERSION));
        sceIoClose(f);
    }
}

static int starter(SceSize args, void *argp) {
    (void)args; (void)argp;
    /* JavaTask has to be waiting on its event queue before the start event lands */
    sceKernelDelayThread(500000);
    javanotify_start();
    return 0;
}

int main(int argc, char *argv[]) {
    SceUID th;
    (void)argc; (void)argv;

    sceIoMkdir(J2V_DATA, 0777);
    javacall_log_open();
    javacall_printf("J2Vita %s\n", J2V_VERSION);

    /* the C interpreter is CPU bound, run the ARM cores at full speed */
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);

    seed_data();
    cfg_select_game(NULL, NULL);
    video_start();

    th = sceKernelCreateThread("j2v_start", starter, 0x10000100, 0x4000, 0, 0, NULL);
    if (th >= 0)
        sceKernelStartThread(th, 0, NULL);

    JavaTask();

    javacall_printf("JavaTask returned\n");
    javacall_log_flush();
    sceKernelExitProcess(0);
    return 0;
}
