#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <stdint.h>
#include "javacall_events.h"
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QCAP 32
typedef struct { unsigned char *data; int len; } EventSlot;
static EventSlot q[QCAP];
static int head, tail, count;
static SceUID mutex_id = -1, items_id = -1;

/* time the VM spent blocked here with nothing to run, for the performance OSD */
volatile uint64_t j2v_vm_idle_us;

static int lockq(void) { return sceKernelWaitSema(mutex_id, 1, NULL); }
static void unlockq(void) { sceKernelSignalSema(mutex_id, 1); }

javacall_result javacall_event_receive(long wait_ms, unsigned char* buffer, int maxlen, int* outlen) {
    SceUInt timeout, *pto = NULL;
    int rc;
    if (outlen) *outlen = 0;
    if (items_id < 0 || mutex_id < 0) return JAVACALL_FAIL;
    if (wait_ms == 0) rc = sceKernelPollSema(items_id, 1);
    else {
        SceUInt64 t0 = sceKernelGetProcessTimeWide();
        if (wait_ms > 0) { timeout = (SceUInt)wait_ms * 1000U; pto = &timeout; }
        rc = sceKernelWaitSema(items_id, 1, pto);
        j2v_vm_idle_us += sceKernelGetProcessTimeWide() - t0;
    }
    if (rc < 0) return JAVACALL_FAIL;
    if (lockq() < 0) return JAVACALL_FAIL;
    if (count <= 0) { unlockq(); return JAVACALL_FAIL; }
    EventSlot e = q[head]; q[head].data = NULL; q[head].len = 0;
    head = (head + 1) % QCAP; count--;
    unlockq();
    if (e.len > maxlen) { free(e.data); return JAVACALL_OUT_OF_MEMORY; }
    memcpy(buffer, e.data, e.len); if (outlen) *outlen = e.len; free(e.data);
    return JAVACALL_OK;
}

javacall_result javacall_event_send(unsigned char* buffer, int len) {
    unsigned char *copy;
    if (!buffer || len < 0 || items_id < 0 || mutex_id < 0) return JAVACALL_FAIL;
    copy = (unsigned char*)malloc((size_t)len); if (!copy) return JAVACALL_FAIL;
    memcpy(copy, buffer, (size_t)len);
    if (lockq() < 0) { free(copy); return JAVACALL_FAIL; }
    if (count == QCAP) { unlockq(); free(copy); return JAVACALL_FAIL; }
    q[tail].data = copy; q[tail].len = len; tail = (tail + 1) % QCAP; count++;
    unlockq(); sceKernelSignalSema(items_id, 1); return JAVACALL_OK;
}

javacall_result javacall_events_init(void) {
    int i; head = tail = count = 0; for (i=0;i<QCAP;i++){q[i].data=NULL;q[i].len=0;}
    mutex_id = sceKernelCreateSema("pspkvm_evt_lock", 0, 1, 1, NULL);
    items_id = sceKernelCreateSema("pspkvm_evt_items", 0, 0, QCAP, NULL);
    return (mutex_id >= 0 && items_id >= 0) ? JAVACALL_OK : JAVACALL_FAIL;
}

javacall_result javacall_events_finalize(void) {
    int i; if (mutex_id >= 0) lockq();
    for (i=0;i<QCAP;i++) { free(q[i].data); q[i].data=NULL; q[i].len=0; }
    if (mutex_id >= 0) { unlockq(); sceKernelDeleteSema(mutex_id); mutex_id=-1; }
    if (items_id >= 0) { sceKernelDeleteSema(items_id); items_id=-1; }
    head=tail=count=0; return JAVACALL_OK;
}
#ifdef __cplusplus
}
#endif
