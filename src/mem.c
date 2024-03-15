#include "t4/mem.h"

#include <stdlib.h>
#include <string.h>

#if !defined(T4_DEBUG)

#include <stdio.h>

void * t4_calloc_debug(const size_t n, const size_t size, const char * file, const int line)
{
    void * ptr = calloc(n, size);
    if (ptr == NULL) {
        fprintf(stderr, "t4_calloc(n: %lu, size: %lu) failed: %s:%d\n", n, size, file, line);
        abort();
    }
    return ptr;
}

void * t4_calloc_aligned_debug(const size_t size, const size_t alignment, const char * file, const int line)
{
    void * ptr = aligned_alloc(alignment, size);
    if (ptr == NULL) {
        fprintf(stderr, "t4_calloc(size: %lu, alignment: %lu) failed: %s:%d\n", size, alignment, file, line);
        abort();
    }

    memset(ptr, 0, size);

    return ptr;
}

#else

#include <assert.h>

void * t4_calloc(size_t n, size_t size)
{
    void * ptr = calloc(n, size);
    assert(ptr != NULL);
    return ptr;
}

void * t4_calloc_aligned(size_t n, size_t size, size_t alignment)
{
    void * ptr = aligned_alloc(alignment, size * n);
    assert(ptr != NULL);

    memset(ptr, 0, size * n);

    return ptr;
}

#endif /* T4_DEBUG */
