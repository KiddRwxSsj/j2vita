#include <stddef.h>
#include <kni.h>
#include <commonKNIMacros.h>
#include "javacall_lifecycle.h"

/* vita/config.c */
void cfg_select_game(const char *name, const char *hint);
void menu_open(void);
unsigned int theme_color(int slot);
extern int j2v_theme_serial;
int cfg_device_index(void);

static void ascii(jobject str, char *out, int max) {
    jchar buf[256];
    int len = 0, i;
    if (!KNI_IsNullHandle(str)) {
        len = KNI_GetStringLength(str);
        if (len > max - 1)
            len = max - 1;
        if (len > 256)
            len = 256;
        KNI_GetStringRegion(str, 0, len, buf);
    }
    for (i = 0; i < len; i++)
        out[i] = buf[i] < 128 ? (char)buf[i] : '_';
    out[len] = 0;
}

KNIEXPORT KNI_RETURNTYPE_INT
KNIDECL(com_pspkvm_system_J2Vita_beginGame) {
    char name[64], hint[256];
    int dev = -1;

    KNI_StartHandles(2);
    KNI_DeclareHandle(str);
    KNI_DeclareHandle(hstr);
    KNI_GetParameterAsObject(1, str);
    KNI_GetParameterAsObject(2, hstr);
    if (KNI_IsNullHandle(str)) {
        cfg_select_game(NULL, NULL);
    } else {
        ascii(str, name, sizeof(name));
        ascii(hstr, hint, sizeof(hint));
        cfg_select_game(name, hint);
        dev = cfg_device_index();
    }
    KNI_EndHandles();
    KNI_ReturnInt(dev);
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_pspkvm_system_J2Vita_quit) {
    javanotify_shutdown();
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_pspkvm_system_J2Vita_openSettings) {
    menu_open();
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_INT
KNIDECL(com_pspkvm_system_J2Vita_themeColor) {
    KNI_ReturnInt((jint)theme_color(KNI_GetParameterAsInt(1)));
}

KNIEXPORT KNI_RETURNTYPE_INT
KNIDECL(com_pspkvm_system_J2Vita_themeSerial) {
    KNI_ReturnInt(j2v_theme_serial);
}
