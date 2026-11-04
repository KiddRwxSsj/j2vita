#include <kni.h>
#include <sni.h>
#include <midpMalloc.h>
#include <midpDataHash.h>
#include <midpError.h>
#include <midpUtilKni.h>
#include <midpStorage.h>
#include <commonKNIMacros.h>
#include <midpJar.h>
#include <suitestore_installer.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>

KNIEXPORT KNI_RETURNTYPE_INT
KNIDECL(com_sun_midp_installer_SuiteDownloadInfo_openDir) {
    int handle;
    
    KNI_StartHandles(1);
    GET_PARAMETER_AS_PCSL_STRING(1, dirName)

    handle = (int)storage_open_file_iterator(&dirName);

    RELEASE_PCSL_STRING_PARAMETER

    KNI_EndHandles();
    KNI_ReturnInt(handle);
}

KNIEXPORT KNI_RETURNTYPE_OBJECT
KNIDECL(com_sun_midp_installer_SuiteDownloadInfo_nextFileInDir0) {
    int handle, f_errc;
    pcsl_string filename = PCSL_STRING_NULL;
    
    handle = KNI_GetParameterAsInt(2);
    
    KNI_StartHandles(2);
    KNI_DeclareHandle(ret);

    GET_PARAMETER_AS_PCSL_STRING(1, root)
    
    f_errc = storage_get_next_file_in_iterator(&root, (void*)handle, &filename);
    if (0 == f_errc) {
            midp_jstring_from_pcsl_string(KNIPASSARGS &filename, ret);
    } else {
            KNI_ReleaseHandle(ret);
    }
    
    RELEASE_PCSL_STRING_PARAMETER
    
    
    pcsl_string_free(&filename);
    KNI_EndHandlesAndReturnObject(ret);
}

extern unsigned short JIS_UNICODE[];

KNIEXPORT KNI_RETURNTYPE_OBJECT
KNIDECL(com_sun_midp_installer_SuiteDownloadInfo_convert2lable0) {
    pcsl_string to = PCSL_STRING_NULL;
    int i, len, chars = 0;
    
    KNI_StartHandles(2);
    KNI_DeclareHandle(ret);

    GET_PARAMETER_AS_PCSL_STRING(1, from)
#ifdef PSP
    len = pcsl_string_length(&from);
    to.data = midpMalloc((len + 1)*sizeof(jchar));
    if (to.data) {
        to.flags = PCSL_STRING_IN_HEAP;
     
        for (i = 0; i < len; ) {
        	 if ((from.data[i] > 0 && from.data[i] < 128) || (i+1 == len)) {
        	     to.data[chars] = from.data[i];
        	     i+=1;
        	 } else {
        	     to.data[chars] = JIS_UNICODE[(from.data[i] << 8) | from.data[i+1]];
        	     i+=2;
        	 }
        	 chars++;
        }
        to.data[chars] = 0;
        to.length = chars + 1;
        
        midp_jstring_from_pcsl_string(KNIPASSARGS &to, ret);
        
        RELEASE_PCSL_STRING_PARAMETER
        
        
        pcsl_string_free(&to);
    }
#else
    midp_jstring_from_pcsl_string(KNIPASSARGS &from, ret);
        
    RELEASE_PCSL_STRING_PARAMETER    
#endif
    KNI_EndHandlesAndReturnObject(ret);
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_sun_midp_installer_SuiteDownloadInfo_closeDir) {
    int handle;
    
    handle = KNI_GetParameterAsInt(1);
    storageCloseFileIterator((void*)handle);
    KNI_ReturnVoid();
}


/* midpIterateJarEntries has no user pointer, the sync thread is the only caller */
static char zip_names[4096];
static int zip_len;

static jboolean zip_filter(const pcsl_string *name) {
    jbyte buf[512];
    jsize n = 0;
    if (pcsl_string_convert_to_utf8(name, buf, sizeof(buf) - 1, &n) != PCSL_STRING_OK || n < 5) return KNI_FALSE;
    buf[n] = 0;
    char *ext = (char *)buf + n - 4;
    return strcasecmp(ext, ".jar") == 0 || strcasecmp(ext, ".jad") == 0;
}

static jboolean zip_add(const pcsl_string *name) {
    jsize n = 0;
    if (pcsl_string_convert_to_utf8(name, (jbyte *)zip_names + zip_len,
            sizeof(zip_names) - zip_len - 2, &n) != PCSL_STRING_OK) return KNI_FALSE;
    zip_len += n;
    zip_names[zip_len++] = '\n';
    zip_names[zip_len] = 0;
    return KNI_TRUE;
}

/* JAR and JAD entry names of a zip, one per line, or null when it can't be read */
KNIEXPORT KNI_RETURNTYPE_OBJECT
KNIDECL(com_sun_midp_installer_SuiteDownloadInfo_zipEntries0) {
    int err = 0;
    void *zip;

    KNI_StartHandles(2);
    KNI_DeclareHandle(ret);
    GET_PARAMETER_AS_PCSL_STRING(1, path)

    zip_len = 0;
    zip_names[0] = 0;
    zip = midpOpenJar(&err, &path);
    if (zip != NULL) {
        if (midpIterateJarEntries(zip, zip_filter, zip_add) == 1) {
            KNI_NewStringUTF(zip_names, ret);
        } else {
            KNI_ReleaseHandle(ret);
        }
        midpCloseJar(zip);
    } else {
        KNI_ReleaseHandle(ret);
    }

    RELEASE_PCSL_STRING_PARAMETER
    KNI_EndHandlesAndReturnObject(ret);
}

static int put_num(jchar *o, long long v) {
    char b[24];
    int n = snprintf(b, sizeof(b), "%lld", v), i;
    for (i = 0; i < n; i++)
        o[i] = (jchar)b[i];
    return n;
}

/*
 * The folders, JARs and zips in dir as "size\tname\n" lines, folders with
 * size -2, null when dir can't be opened. sceIoDread already carries the
 * size and type, so this is one call instead of two stats per entry.
 * Java paths hold one byte per char, the same as javacall's file layer.
 */
KNIEXPORT KNI_RETURNTYPE_OBJECT
KNIDECL(com_sun_midp_installer_SuiteDownloadInfo_listDir0) {
    char path[512];
    jchar *out = NULL;
    int used = 0, cap = 0, len, i;
    SceUID d = -1;
    SceIoDirent e;

    KNI_StartHandles(2);
    KNI_DeclareHandle(dir);
    KNI_DeclareHandle(ret);
    KNI_GetParameterAsObject(1, dir);
    len = KNI_GetStringLength(dir);
    if (len > 0 && len < (int)sizeof(path)) {
        jchar tmp[512];
        KNI_GetStringRegion(dir, 0, len, tmp);
        for (i = 0; i < len; i++)
            path[i] = (char)tmp[i];
        if (len > 1 && path[len - 1] == '/')
            len--;
        path[len] = 0;
        d = sceIoDopen(path);
    }
    if (d >= 0) {
        memset(&e, 0, sizeof(e));
        while (sceIoDread(d, &e) > 0) {
            int n = (int)strlen(e.d_name), isdir = SCE_S_ISDIR(e.d_stat.st_mode);
            if (!isdir && (n < 5 || (strcasecmp(e.d_name + n - 4, ".jar") && strcasecmp(e.d_name + n - 4, ".zip"))))
                continue;
            if (used + n + 32 > cap) {
                jchar *t = (jchar *)realloc(out, (cap = (cap + n + 32) * 2) * sizeof(jchar));
                if (t == NULL)
                    break;
                out = t;
            }
            used += put_num(out + used, isdir ? -2 : (long long)e.d_stat.st_size);
            out[used++] = '\t';
            for (i = 0; i < n; i++)
                out[used++] = (jchar)(unsigned char)e.d_name[i];
            out[used++] = '\n';
        }
        sceIoDclose(d);
        KNI_NewString(out, used, ret);
    } else {
        KNI_ReleaseHandle(ret);
    }
    free(out);
    KNI_EndHandlesAndReturnObject(ret);
}

/* size in bytes, or -1; stat is far cheaper than opening every JAR on each poll */
KNIEXPORT KNI_RETURNTYPE_INT
KNIDECL(com_sun_midp_installer_SuiteDownloadInfo_fileSize0) {
    int size = -1;
    struct stat st;

    KNI_StartHandles(1);
    GET_PARAMETER_AS_PCSL_STRING(1, path)
    {
        const char *p = (const char *)pcsl_string_get_utf8_data(&path);
        if (p != NULL) {
            if (stat(p, &st) == 0) size = (int)st.st_size;
            pcsl_string_release_utf8_data((const jbyte *)p, &path);
        }
    }
    RELEASE_PCSL_STRING_PARAMETER
    KNI_EndHandles();
    KNI_ReturnInt(size);
}

static unsigned fnv(unsigned h, const char *s) {
    for (; *s; s++)
        h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

/* names and sizes of the JARs, zips and folders below dir, folded into one number */
static unsigned dir_hash(const char *dir, int depth, unsigned h) {
    SceIoDirent e;
    SceUID d = sceIoDopen(dir);
    if (d < 0)
        return fnv(h, "?");
    memset(&e, 0, sizeof(e));
    while (sceIoDread(d, &e) > 0) {
        char buf[32];
        int n = (int)strlen(e.d_name);
        if (e.d_name[0] == '.')
            continue;
        if (SCE_S_ISDIR(e.d_stat.st_mode)) {
            if (depth > 1) {
                char sub[512];
                snprintf(sub, sizeof(sub), "%s/%s", dir, e.d_name);
                h = fnv(fnv(h, "/"), e.d_name);
                h = dir_hash(sub, depth - 1, h);
            }
        } else if (n > 4 && (!strcasecmp(e.d_name + n - 4, ".jar") || !strcasecmp(e.d_name + n - 4, ".zip"))) {
            snprintf(buf, sizeof(buf), "|%lld|", (long long)e.d_stat.st_size);
            h = fnv(fnv(h, e.d_name), buf);
        }
    }
    sceIoDclose(d);
    return fnv(h, "<");
}

KNIEXPORT KNI_RETURNTYPE_INT
KNIDECL(com_sun_midp_installer_SuiteDownloadInfo_dirHash0) {
    unsigned h = 2166136261u;
    int depth = KNI_GetParameterAsInt(2);

    KNI_StartHandles(1);
    GET_PARAMETER_AS_PCSL_STRING(1, path)
    {
        const char *p = (const char *)pcsl_string_get_utf8_data(&path);
        if (p != NULL) {
            char dir[512];
            int n;
            snprintf(dir, sizeof(dir), "%s", p);
            n = (int)strlen(dir);
            if (n > 0 && dir[n - 1] == '/')
                dir[n - 1] = 0;
            h = dir_hash(dir, depth, h);
            pcsl_string_release_utf8_data((const jbyte *)p, &path);
        }
    }
    RELEASE_PCSL_STRING_PARAMETER
    KNI_EndHandles();
    KNI_ReturnInt((jint)h);
}

/* a moved JAR: the suite keeps its id, its copy and its saves, only the source url changes */
KNIEXPORT KNI_RETURNTYPE_BOOLEAN
KNIDECL(com_sun_midp_installer_SuiteDownloadInfo_setJarUrl0) {
    jboolean ok = KNI_FALSE;
    SuiteIdType id = KNI_GetParameterAsInt(1);

    KNI_StartHandles(1);
    GET_PARAMETER_AS_PCSL_STRING(2, url)
    {
        ok = midp_set_suite_jar_url(id, &url) == ALL_OK ? KNI_TRUE : KNI_FALSE;
    }
    RELEASE_PCSL_STRING_PARAMETER
    KNI_EndHandles();
    KNI_ReturnBoolean(ok);
}

KNIEXPORT KNI_RETURNTYPE_BOOLEAN
KNIDECL(com_sun_midp_installer_SuiteDownloadInfo_saveSuites0) {
    KNI_ReturnBoolean(midp_save_suites_data() == ALL_OK ? KNI_TRUE : KNI_FALSE);
}

/* "jar path\tName|Vendor\tjar size" from the suite list, or null */
KNIEXPORT KNI_RETURNTYPE_OBJECT
KNIDECL(com_sun_midp_installer_SuiteDownloadInfo_suiteJar0) {
    SuiteIdType id = KNI_GetParameterAsInt(1);
    pcsl_string jar, key, tab = PCSL_STRING_NULL, size = PCSL_STRING_NULL;
    pcsl_string jarTab = PCSL_STRING_NULL, keyed = PCSL_STRING_NULL, keyTab = PCSL_STRING_NULL, all = PCSL_STRING_NULL;
    jint jarSize;
    char num[16];

    KNI_StartHandles(1);
    KNI_DeclareHandle(ret);
    if (midp_get_suite_jar_and_key(id, &jar, &key, &jarSize) == ALL_OK) {
        snprintf(num, sizeof(num), "%ld", (long)jarSize);
        if (pcsl_string_convert_from_utf8((const jbyte*)"\t", 1, &tab) == PCSL_STRING_OK &&
                pcsl_string_convert_from_utf8((const jbyte*)num, (jint)strlen(num), &size) == PCSL_STRING_OK &&
                pcsl_string_cat(&jar, &tab, &jarTab) == PCSL_STRING_OK &&
                pcsl_string_cat(&jarTab, &key, &keyed) == PCSL_STRING_OK &&
                pcsl_string_cat(&keyed, &tab, &keyTab) == PCSL_STRING_OK &&
                pcsl_string_cat(&keyTab, &size, &all) == PCSL_STRING_OK) {
            midp_jstring_from_pcsl_string(KNIPASSARGS &all, ret);
        } else {
            KNI_ReleaseHandle(ret);
        }
        pcsl_string_free(&jar);
        pcsl_string_free(&key);
    } else {
        KNI_ReleaseHandle(ret);
    }
    pcsl_string_free(&tab);
    pcsl_string_free(&size);
    pcsl_string_free(&jarTab);
    pcsl_string_free(&keyed);
    pcsl_string_free(&keyTab);
    pcsl_string_free(&all);
    KNI_EndHandlesAndReturnObject(ret);
}
