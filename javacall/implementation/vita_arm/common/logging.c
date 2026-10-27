#include "javacall_logging.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>

/*
 * Everything (javacall, VM, System.out, frontend) ends up in ux0:data/j2vita/log.txt.
 * Writes are batched because some games print every frame, javacall_log_flush()
 * is called once a second by the frontend. Until then every line is written
 * straight away so boot crashes still leave a trace.
 */
#define LOG_PATH "ux0:data/j2vita/log.txt"

static SceUID fd = -1;
static int ready, immediate = 1;
static SceKernelLwMutexWork lock;
static char buf[8192];
static int used;

static void flush_locked(void) {
    if (used > 0 && fd >= 0)
        sceIoWrite(fd, buf, used);
    used = 0;
}

void javacall_log_open(void) {
    if (ready)
        return;
    sceKernelCreateLwMutex(&lock, "j2v_log", 0, 0, NULL);
    fd = sceIoOpen(LOG_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    ready = 1;
}

void javacall_log_flush(void) {
    if (!ready)
        return;
    sceKernelLockLwMutex(&lock, 1, NULL);
    flush_locked();
    sceKernelUnlockLwMutex(&lock, 1);
}

void javacall_log_batch(void) {
    immediate = 0;
}

void javacall_print(const char *s) {
    int n;
    if (!s)
        return;
    if (!ready)
        javacall_log_open();
    n = (int)strlen(s);
    sceKernelLockLwMutex(&lock, 1, NULL);
    if (used + n > (int)sizeof(buf))
        flush_locked();
    if (n > (int)sizeof(buf)) {
        if (fd >= 0)
            sceIoWrite(fd, s, n);
    } else {
        memcpy(buf + used, s, n);
        used += n;
    }
    if (immediate)
        flush_locked();
    sceKernelUnlockLwMutex(&lock, 1);
}

void javacall_printf(const char* format, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, format);
    vsnprintf(tmp, sizeof(tmp), format, ap);
    va_end(ap);
    javacall_print(tmp);
}

void javacall_logging_channel(int channel) {
    (void)channel;
}
