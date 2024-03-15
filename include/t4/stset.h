#ifndef T4_STSET_H_
#define T4_STSET_H_

#include "t4/common.h"
#include "t4/internal/stset_vtable.h"

// TODO Consider adding flags to disable dynamic dispatch

typedef struct t4_stset_entry {
    u64 hash;
    void * data;
    size_t size;
} t4_stset_entry_t;

typedef struct t4_stset {
    size_t capacity;
    t4_stset_entry_t * entries;

    /* This must to be aligned */
    u8 * metadata;
} t4_stset_t;

/**
 * @brief First call is not thread-safe; the first call to either this or @ref t4_stset_new
 * will initialise the internal vtable and set the seed.
 *
 * @return The alignment used for SIMD operations, eg. AVX2: 32
 */
static inline size_t t4_stset_get_alignment(void) {
    return t4_internal_stset_vtable.get_alignment();
}

/**
 * @brief First call is not thread-safe; the first call to either this or @ref t4_stset_get_alignment
 * will initialise the internal vtable and set the seed.
 *
 * @param capacity Initial capacity for the set (1024 at minimum)
 * @return The newly created stset instance
 */
static inline t4_stset_t t4_stset_new(const size_t capacity) {
    return t4_internal_stset_vtable.new(capacity);
}

static inline void t4_stset_free(t4_stset_t * self) {
    t4_internal_stset_vtable.free(self);
}

static inline void t4_stset_insert_unchecked(t4_stset_t * self, void * data, const size_t data_size) {
    t4_internal_stset_vtable.insert_unchecked(self, data, data_size);
}

static inline bool t4_stset_try_insert(t4_stset_t * self, void * data, const size_t data_size) {
    return t4_internal_stset_vtable.try_insert(self, data, data_size);
}

static inline bool t4_stset_exists(const t4_stset_t * self, const void * data, const size_t data_size) {
    return t4_internal_stset_vtable.exists(self, data, data_size);
}

#endif /* T4_STSET_H_ */