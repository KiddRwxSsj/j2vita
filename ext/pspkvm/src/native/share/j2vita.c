#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <kni.h>
#include <commonKNIMacros.h>
#include "javacall_lifecycle.h"

/* vita/config.c */
void cfg_select_game(const char *name, const char *hint);
void menu_open(void);
unsigned int theme_color(int slot);
extern int j2v_theme_serial;
int cfg_device_index(void);
void ime_ask(const uint16_t *title, const uint16_t *initial);
int ime_ask_state(void);
const uint16_t *ime_answer(void);

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


static void wide(jobject str, jchar *out, int max) {
    int len = KNI_IsNullHandle(str) ? 0 : KNI_GetStringLength(str);
    if (len > max - 1)
        len = max - 1;
    if (len > 0)
        KNI_GetStringRegion(str, 0, len, out);
    out[len] = 0;
}

/* Java string as a UTF-8 path */
static void utf8(jobject str, char *out, int max) {
    jchar buf[256];
    int i, n = 0;
    wide(str, buf, 256);
    for (i = 0; buf[i] && n < max - 4; i++) {
        unsigned c = buf[i];
        if (c < 0x80) {
            out[n++] = (char)c;
        } else if (c < 0x800) {
            out[n++] = (char)(0xC0 | c >> 6);
            out[n++] = (char)(0x80 | (c & 0x3F));
        } else {
            out[n++] = (char)(0xE0 | c >> 12);
            out[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    out[n] = 0;
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_pspkvm_system_J2Vita_askText) {
    jchar title[65], init[65];
    KNI_StartHandles(2);
    KNI_DeclareHandle(t);
    KNI_DeclareHandle(i);
    KNI_GetParameterAsObject(1, t);
    KNI_GetParameterAsObject(2, i);
    wide(t, title, 65);
    wide(i, init, 65);
    ime_ask(title, init);
    KNI_EndHandles();
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_INT
KNIDECL(com_pspkvm_system_J2Vita_textState) {
    KNI_ReturnInt(ime_ask_state());
}

KNIEXPORT KNI_RETURNTYPE_OBJECT
KNIDECL(com_pspkvm_system_J2Vita_textResult) {
    const uint16_t *a = ime_answer();
    int n = 0;
    KNI_StartHandles(1);
    KNI_DeclareHandle(ret);
    while (n < 64 && a[n])
        n++;
    KNI_NewString((const jchar *)a, n, ret);
    KNI_EndHandlesAndReturnObject(ret);
}

KNIEXPORT KNI_RETURNTYPE_BOOLEAN
KNIDECL(com_pspkvm_system_J2Vita_makeDir) {
    char path[512];
    KNI_StartHandles(1);
    KNI_DeclareHandle(p);
    KNI_GetParameterAsObject(1, p);
    utf8(p, path, sizeof(path));
    KNI_EndHandles();
    KNI_ReturnBoolean(mkdir(path, 0777) == 0);
}

KNIEXPORT KNI_RETURNTYPE_BOOLEAN
KNIDECL(com_pspkvm_system_J2Vita_removeDir) {
    char path[512];
    KNI_StartHandles(1);
    KNI_DeclareHandle(p);
    KNI_GetParameterAsObject(1, p);
    utf8(p, path, sizeof(path));
    KNI_EndHandles();
    KNI_ReturnBoolean(rmdir(path) == 0);
}

/* real rename of a file or folder, fails if the target exists */
KNIEXPORT KNI_RETURNTYPE_BOOLEAN
KNIDECL(com_pspkvm_system_J2Vita_move) {
    char from[512], to[512];
    struct stat st;
    int ok = 0;
    KNI_StartHandles(2);
    KNI_DeclareHandle(a);
    KNI_DeclareHandle(b);
    KNI_GetParameterAsObject(1, a);
    KNI_GetParameterAsObject(2, b);
    utf8(a, from, sizeof(from));
    utf8(b, to, sizeof(to));
    if (stat(to, &st) != 0)
        ok = rename(from, to) == 0;
    KNI_EndHandles();
    KNI_ReturnBoolean(ok);
}

/* n big endian words of src from byte off into dst, for caches that would take a Java loop per pixel */
KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_pspkvm_system_J2Vita_unpackInts) {
    jint off = KNI_GetParameterAsInt(2), n = KNI_GetParameterAsInt(4), done = 0;
    unsigned char in[4096];
    jint out[1024];
    KNI_StartHandles(2);
    KNI_DeclareHandle(src);
    KNI_DeclareHandle(dst);
    KNI_GetParameterAsObject(1, src);
    KNI_GetParameterAsObject(3, dst);
    if (!KNI_IsNullHandle(src) && !KNI_IsNullHandle(dst) && off >= 0 && n > 0 &&
        n <= (KNI_GetArrayLength(src) - off) / 4 && n <= KNI_GetArrayLength(dst)) {
        while (done < n) {
            int k = n - done > 1024 ? 1024 : n - done, i;
            KNI_GetRawArrayRegion(src, off + done * 4, k * 4, (jbyte *)in);
            for (i = 0; i < k; i++)
                out[i] = (jint)((unsigned)in[i * 4] << 24 | in[i * 4 + 1] << 16 | in[i * 4 + 2] << 8 | in[i * 4 + 3]);
            KNI_SetRawArrayRegion(dst, done * 4, k * 4, (const jbyte *)out);
            done += k;
        }
    }
    KNI_EndHandles();
    KNI_ReturnVoid();
}

/* UTF-8 bytes to a String, CLDC's own decoder is a Java loop per byte */
KNIEXPORT KNI_RETURNTYPE_OBJECT
KNIDECL(com_pspkvm_system_J2Vita_utf8) {
    jint off = KNI_GetParameterAsInt(2), len = KNI_GetParameterAsInt(3);
    char *buf = NULL;
    KNI_StartHandles(2);
    KNI_DeclareHandle(src);
    KNI_DeclareHandle(ret);
    KNI_GetParameterAsObject(1, src);
    if (!KNI_IsNullHandle(src) && off >= 0 && len >= 0 && len <= KNI_GetArrayLength(src) - off &&
        (buf = (char *)malloc(len + 1)) != NULL) {
        KNI_GetRawArrayRegion(src, off, len, (jbyte *)buf);
        buf[len] = 0;
        KNI_NewStringUTF(buf, ret);
        free(buf);
    } else {
        KNI_ReleaseHandle(ret);
    }
    KNI_EndHandlesAndReturnObject(ret);
}

void cfg_screen_hint(int w, int h);

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_pspkvm_system_J2Vita_screenHint) {
    cfg_screen_hint(KNI_GetParameterAsInt(1), KNI_GetParameterAsInt(2));
    KNI_ReturnVoid();
}
