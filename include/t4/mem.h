#ifndef T4_MEM_H_
#define T4_MEM_H_

#include "t4/common.h"

#include <stdlib.h>

#define t4_free(ptr) (free((ptr)), *(&(ptr)) = NULL)

#ifndef _WIN32
#define t4_free_aligned(ptr) t4_free(ptr)
#else
// Screw you microsfot and your subpar standard library implementations
// which never implement the optional parts of the standard. Because of
// you I need to have a sucky dedicated API for freeing aligned pointers
// instead of just using the general one. msvcrt's free() cannot handle aligned
// pointers.
#define t4_free_aligned(ptr) (_aligned_free((ptr)), *(&(ptr)) = NULL)
#endif

/* TODO implement this flag */
#if !defined(T4_DEBUG)

void * t4_calloc_debug(size_t n, size_t size, const char * file, int line);
void * t4_calloc_aligned_debug(size_t size, size_t alignment, const char * file, int line);

#define t4_calloc(n, size) t4_calloc_debug((n), (size), __FILE__, __LINE__)

#define t4_calloc_aligned(size, alignment) \
    t4_calloc_aligned_debug((size), (alignment), __FILE__, __LINE__)

#else

// TODO
// void * t4_calloc(size_t n, size_t size);
// void * t4_calloc_aligned(size_t n, size_t size, size_t alignment);

#endif /* !T4_DEBUG */

#define T4_ALIGN_UP(expr, align) ((expr) + (align) - 1) & ~((align) - 1)
#define T4_ALIGN_DOWN(expr, align) (expr) & ~((align) - 1)

#endif /* T4_MEM_H_ */
