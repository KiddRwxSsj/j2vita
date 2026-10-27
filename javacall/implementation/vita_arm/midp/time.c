#include "javacall_time.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#define MAX_TIMERS 8

typedef struct {
    volatile int active;
    volatile unsigned gen;
    int cyclic;
    unsigned usec;
    javacall_callback_func cb;
} Timer;

static Timer timers[MAX_TIMERS];

typedef struct { int slot; unsigned gen; } TimerArg;

/* a thread only serves the generation it was started for, so a reused slot never gets two tickers */
static int timer_thread(SceSize args, void *argp) {
    TimerArg a = *(TimerArg *)argp;
    Timer *t = &timers[a.slot];
    (void)args;

    do {
        unsigned left = t->usec;
        while (left && t->active && t->gen == a.gen) {
            unsigned step = left > 20000 ? 20000 : left;
            sceKernelDelayThread(step);
            left -= step;
        }
        if (!t->active || t->gen != a.gen)
            break;
        t->cb((javacall_handle)(intptr_t)a.slot);
    } while (t->cyclic && t->active && t->gen == a.gen);

    if (t->gen == a.gen)
        t->active = 0;
    sceKernelExitDeleteThread(0);
    return 0;
}

javacall_result javacall_time_initialize_timer(int ms, javacall_bool cyclic,
                                                javacall_callback_func func,
                                                javacall_handle *handle) {
    TimerArg a;
    SceUID th;
    int i;

    if (ms <= 0 || !func || !handle)
        return JAVACALL_FAIL;
    for (i = 0; i < MAX_TIMERS && timers[i].active; i++)
        ;
    if (i == MAX_TIMERS)
        return JAVACALL_FAIL;

    timers[i].cyclic = cyclic == JAVACALL_TRUE;
    timers[i].usec = (unsigned)ms * 1000u;
    timers[i].cb = func;
    timers[i].gen++;
    timers[i].active = 1;
    a.slot = i;
    a.gen = timers[i].gen;

    th = sceKernelCreateThread("j2v_timer", timer_thread, 0x10000100, 0x8000, 0, 0, NULL);
    if (th < 0 || sceKernelStartThread(th, sizeof(a), &a) < 0) {
        if (th >= 0)
            sceKernelDeleteThread(th);
        timers[i].active = 0;
        return JAVACALL_FAIL;
    }
    *handle = (javacall_handle)(intptr_t)i;
    return JAVACALL_OK;
}

javacall_result javacall_time_finalize_timer(javacall_handle handle) {
    int i = (int)(intptr_t)handle;
    if (i < 0 || i >= MAX_TIMERS)
        return JAVACALL_FAIL;
    timers[i].gen++;
    timers[i].active = 0;
    return JAVACALL_OK;
}

char* javacall_time_get_local_timezone(void) {
    static char tz[] = "GMT+00:00";
    return tz;
}

javacall_int64 javacall_time_get_milliseconds_since_1970(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (javacall_int64)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

javacall_time_seconds javacall_time_get_seconds_since_1970(void) {
    return (javacall_time_seconds)time(NULL);
}

javacall_time_milliseconds javacall_time_get_clock_milliseconds(void) {
    return (javacall_time_milliseconds)(sceKernelGetProcessTimeWide() / 1000);
}
