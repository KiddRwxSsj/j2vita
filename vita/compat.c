#include <stddef.h>
#include <string.h>

/* the ARM interpreter loop would provide these, the C interpreter build does not */
void fast_memclear(void *start, int length) {
    if (start != NULL && length > 0)
        memset(start, 0, (size_t)length);
}

int *_rom_constant_pool_fast = NULL;

/* PSPKVM's persistent VMSettings store is PSP only, everything falls back to defaults */
int vmsettings_getint(const char *key, int default_value) {
    (void)key;
    return default_value;
}

int vmsettings_key_equals(const char *key, const char *value) {
    (void)key;
    (void)value;
    return 0;
}

/* read by com.pspkvm.keypad.RawState */
unsigned char ext_pspkvm_rawstate_lx = 128;
unsigned char ext_pspkvm_rawstate_ly = 128;
