/*
 *
 * Copyright  1990-2007 Sun Microsystems, Inc. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 only, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is
 * included at /legal/license.txt).
 * 
 * You should have received a copy of the GNU General Public License
 * version 2 along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 or visit www.sun.com if you need additional
 * information or have any questions.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "stdlib.h"
#include "javacall_defs.h"    
#include "javacall_memory.h"
#include "javacall_properties.h"
#include "javacall_logging.h"
#include "malloc.h"

static int max_heap_size = 0;

/** 
 * Allocates large memory heap
 * VM will use this memory heap for internal memory allocation/deallocation
 * Will be called ONCE during VM startup!
 * 
 * @param    size required heap size in bytes
 * @param    outSize actual size of memory allocated
 * @return	  a pointer to the newly allocated memory, or <tt>0</tt> if not available
 */
/* J2ME games rarely need more than a few MB, 32 MB leaves room for the AMS plus a game */
#define JAVA_HEAP_MAX (32 * 1024 * 1024)
#define JAVA_HEAP_MIN (8 * 1024 * 1024)

void* javacall_memory_heap_allocate(int size, /*OUT*/ int* outSize) {
    int sz = JAVA_HEAP_MAX;
    char* p;

    if (sz < size) {
        sz = size;
    }
    for (; sz >= JAVA_HEAP_MIN && sz >= size; sz -= 1024 * 1024) {
        if ((p = malloc(sz)) != NULL) {
            javacall_printf("java heap: %d bytes\n", sz);
            *outSize = sz;
            max_heap_size = sz;
            return p;
        }
    }
    return NULL;
}

void javacall_memory_heap_deallocate(void* heap) {
    free(heap);
}
    

/** 
 * Allocates memory of the given size from the private JAVACALL memory
 * pool.
 * 
 * @param    size Number of byte to allocate
 * @return	  a pointer to the newly allocated memory
 */
void* /*OPTIONAL*/ javacall_malloc(unsigned int size){ 
    return malloc(size) ;
}

/**
 * Reallocates memory of the given size from the private JAVACALL memory
 * pool.
 *
 * @param size Number of byte to allocate
 * @param ptr	Pointer to previously allocated memory
 * @return a pointer to the reallocated memory or null if memory could not be reallocated
 */
void* /*OPTIONAL*/  javacall_realloc(void* ptr, unsigned int size) {
    return realloc(ptr, size);
}

    
/**
 * Frees memory at the given pointer in the private JAVACALL memory pool.
 * 
 * @param    ptr	Pointer to allocated memory
 */
void  /*OPTIONAL*/ javacall_free(void* ptr) {
    free(ptr);
}

int javacall_total_heap_size() {
    return max_heap_size;
}
    
#ifdef __cplusplus
}
#endif

 
