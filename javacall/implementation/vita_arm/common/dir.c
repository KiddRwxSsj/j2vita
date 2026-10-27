#include "javacall_dir.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
extern char* javacall_UNICODEsToUtf8(const javacall_utf16* fileName, int fileNameLen);

typedef struct {
    DIR *dir;
    char path[JAVACALL_MAX_FILE_NAME_LENGTH + 1];
    javacall_utf16 name[JAVACALL_MAX_FILE_NAME_LENGTH + 2];
} VitaDir;

javacall_handle javacall_dir_open(const javacall_utf16* path, int pathLen) {
    char *p = javacall_UNICODEsToUtf8(path, pathLen);
    VitaDir *h;
    if (!p) return NULL;
    h=(VitaDir*)calloc(1,sizeof(VitaDir)); if(!h) return NULL;
    strncpy(h->path,p,sizeof(h->path)-1);
    h->dir=opendir(p); if(!h->dir){free(h);return NULL;}
    return (javacall_handle)h;
}

void javacall_dir_close(javacall_handle handle) {
    VitaDir *h=(VitaDir*)handle; if(!h)return; if(h->dir)closedir(h->dir); free(h);
}

javacall_utf16* javacall_dir_get_next(javacall_handle handle, int* outFilenameLength) {
    VitaDir *h=(VitaDir*)handle; struct dirent *d; char full[JAVACALL_MAX_FILE_NAME_LENGTH+1]; struct stat st; int len,i,isdir=0;
    if(!h||!h->dir||!outFilenameLength)return NULL;
    d=readdir(h->dir); if(!d)return NULL; len=(int)strlen(d->d_name);
    if(len>=JAVACALL_MAX_FILE_NAME_LENGTH-1)return NULL;
    snprintf(full,sizeof(full),"%s%s%s",h->path,(h->path[0]&&h->path[strlen(h->path)-1]!='/')?"/":"",d->d_name);
    if(stat(full,&st)==0 && S_ISDIR(st.st_mode)) isdir=1;
    for(i=0;i<len;i++) h->name[i]=(javacall_utf16)(unsigned char)d->d_name[i];
    if(isdir){h->name[len++]='/';}
    h->name[len]=0; *outFilenameLength=len; return h->name;
}

javacall_int64 javacall_dir_get_free_space_for_java(void) {
    struct statvfs v; if(statvfs("ux0:data/j2vita",&v)!=0)return 0; return (javacall_int64)v.f_bavail*(javacall_int64)v.f_frsize;
}

javacall_result javacall_dir_get_root_path(javacall_utf16* rootPath, int* rootPathLen) {
    static const char root[]="ux0:data/j2vita"; int i,len=(int)strlen(root);
    if(!rootPath||!rootPathLen||*rootPathLen<len)return JAVACALL_FAIL;
    for(i=0;i<len;i++)rootPath[i]=(javacall_utf16)root[i]; *rootPathLen=len; return JAVACALL_OK;
}

javacall_utf16 javacall_get_file_separator(void) { return (javacall_utf16)'/'; }
javacall_bool javacall_dir_is_secure_storage(javacall_utf16* classPath, int pathLen) { (void)classPath;(void)pathLen;return JAVACALL_FALSE; }
#ifdef __cplusplus
}
#endif
