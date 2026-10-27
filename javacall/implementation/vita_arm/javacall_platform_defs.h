#ifndef __JAVACALL_PLATFORM_DEFINE_H_
#define __JAVACALL_PLATFORM_DEFINE_H_
#include <limits.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint16_t javacall_utf16;
typedef uint8_t javacall_uint8;
typedef uint16_t javacall_uint16;
typedef uint32_t javacall_uint32;
typedef uint64_t javacall_uint64;
typedef int32_t javacall_int32;
typedef int64_t javacall_int64;
#define JAVACALL_MAX_EVENT_SIZE 512
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#define JAVACALL_MAX_FILE_NAME_LENGTH PATH_MAX
#define JAVACALL_MAX_ILLEGAL_FILE_NAME_CHARS 256
#define JAVACALL_MAX_ROOTS_LIST_LENGTH 8192
#define JAVACALL_MAX_ROOT_PATH_LENGTH 256
#define JAVACALL_MAX_LOCALIZED_ROOTS_LIST_LENGTH 8192
#define JAVACALL_MAX_LOCALIZED_DIR_NAME_LENGTH 512
#ifdef __cplusplus
}
#endif
#endif
