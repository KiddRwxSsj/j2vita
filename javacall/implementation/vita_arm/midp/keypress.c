#include "javacall_keypress.h"
#include "javacall_defs.h"

/* implemented by the frontend in vita/input.c */
void input_start(void);
void input_stop(void);

javacall_result javacall_key_init(void) {
    input_start();
    return JAVACALL_OK;
}

javacall_result javacall_key_finalize(void) {
    input_stop();
    return JAVACALL_OK;
}
