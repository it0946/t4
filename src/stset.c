#include "t4/stset.h"

#include "t4/common.h"
#include "t4/rtinfo.h"
#include "t4/mem.h"
#include "t4/wyhash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

/**
 * SSE4_2 support here would basically be a copy paste job of:
 *  - t4_stset_increase_capacity_avx2;
 *  - t4_stset_insert_unchecked_avx2;
 *  - t4_stset_try_insert_avx2;
 *  - t4_stset_exists_avx2;
 * with the only difference being the registers and instructions. Would it *really*
 * be worthwhile supporting two identical implementations, for a 16 byte difference? 
 */
#include <immintrin.h>

#define T4_FILLED ((u8)0x80)

#define T4_GET_H1(hash) ((u64)(hash) >> 7lu)
#define T4_GET_H2(hash) ((u64)(hash) & 0x7flu)

/* Set begin */

/*
 * This alignment is purely for the metadata.
 */
static size_t t4_stset_alignment;

static u64 t4_stset_seed;

/* Makes a H1|H2 hash for use within the set */
static inline u64 t4_make_hash_h1h2(const void * key, const size_t size) {
    const u64 hash = wyhash(key, size, t4_stset_seed, _wyp);

    const u64 h1mask = 0x1fffffffffffffflu;
    const u64 h2mask = 0x7flu;

    const u64 h1 = hash % h1mask;
    const u64 h2 = hash % h2mask;

    return (h1 << 7lu) | h2;
}

static size_t t4_stset_get_alignment_impl(void) {
    return t4_stset_alignment;
}

static t4_stset_t t4_stset_new_aligned(size_t capacity) {
    capacity = capacity < 1024 ? 1024 : T4_ALIGN_UP(capacity, t4_stset_alignment);

    return (t4_stset_t) {
        .capacity = capacity,
        // NOTE doesn't really need to be zeroed, consider switching to a malloc instead
        .entries = t4_calloc(capacity, sizeof(t4_stset_entry_t)),
        .metadata = t4_calloc_aligned(capacity, t4_stset_alignment),
    };
}

static void t4_stset_free_aligned(t4_stset_t * self) {
    t4_free(self->entries);
    t4_free_aligned(self->metadata);
    self->capacity = 0;
}

/* TODO cleanup */
static void t4_stset_increase_capacity_avx2(t4_stset_t * self) {
    const size_t new_capacity = self->capacity * 2;

    u8 * new_metadata = t4_calloc_aligned(new_capacity, t4_stset_alignment);
    t4_stset_entry_t * new_entries = t4_calloc(new_capacity, sizeof(t4_stset_entry_t));

    const __m256i empty = _mm256_set1_epi8(T4_FILLED);

    for (size_t i = 0; i < self->capacity; i++) {
        const t4_stset_entry_t * curr = self->entries + i;

        // TODO look at the asm for this and check if the compiler takes the subq outside of the loop
        u64 j = T4_ALIGN_DOWN(T4_GET_H1(curr->hash) % new_capacity, t4_stset_alignment);

        for (;;) {
            const __m256i candidates = _mm256_load_si256((const __m256i *)(new_metadata + j));
            const u32 matches = _mm256_movemask_epi8(_mm256_andnot_si256(candidates, empty));

            if (matches) {
                j += _tzcnt_u32(matches);
                break;
            }

            j = (j + t4_stset_alignment) % new_capacity;
        }

        new_metadata[j] = T4_GET_H2(curr->hash) | T4_FILLED;

        memcpy(new_entries + j, curr, sizeof(t4_stset_entry_t));
    }

    t4_free_aligned(self->metadata);
    t4_free(self->entries);

    self->metadata = new_metadata;
    self->entries = new_entries;
    self->capacity = new_capacity;
}

static void t4_stset_insert_unchecked_avx2(t4_stset_t * self, void * data, const size_t data_size) {
    const u64 hash = t4_make_hash_h1h2(data, data_size);

    const __m256i r_empty = _mm256_set1_epi8(T4_FILLED);

    const u64 h1 = T4_GET_H1(hash);

    u64 start = T4_ALIGN_DOWN(h1 % self->capacity, t4_stset_alignment);
    u64 i = start;

    for (;;) {
        const __m256i candidates = _mm256_load_si256((const __m256i *)(self->metadata + i));

        /* TODO Apparently the order in which the arguments are passed to _mm256_andnot_si256 matters. Look into why. */
        const u32 empty_mask = _mm256_movemask_epi8(_mm256_andnot_si256(candidates, r_empty));

        if (empty_mask) {
            i += _tzcnt_u32(empty_mask);

            self->metadata[i] = T4_GET_H2(hash) | T4_FILLED;
            self->entries[i] = (t4_stset_entry_t) {
                .hash = hash,
                .data = data,
                .size = data_size,
            };

            return;
        }

        i = (i + t4_stset_alignment) % self->capacity;
        if (i == start) {
            /* TODO on capacity increase, we no longer need this check at all. */
            t4_stset_increase_capacity_avx2(self);
            start = T4_ALIGN_DOWN(h1 % self->capacity, t4_stset_alignment);
            i = start;
        }
    }
}

static bool t4_stset_try_insert_avx2(t4_stset_t * self, void * data, const size_t data_size) {
    const u64 hash = t4_make_hash_h1h2(data, data_size);

    const u64 h1 = T4_GET_H1(hash);
    const u8 h2 = T4_GET_H2(hash) | T4_FILLED;

    const __m256i r_empty = _mm256_set1_epi8(T4_FILLED);
    const __m256i r_h2 = _mm256_set1_epi8(h2);

    u64 start = T4_ALIGN_DOWN(h1 % self->capacity, t4_stset_alignment);
    u64 i = start;

    for (;;) {
        const __m256i candidates = _mm256_load_si256((const __m256i *)(self->metadata + i));

        const u32 empty_mask = _mm256_movemask_epi8(_mm256_andnot_si256(candidates, r_empty));
        u32 match_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(candidates, r_h2));

        const u32 tz_empty = _tzcnt_u32(empty_mask);
        u32 tz_match = _tzcnt_u32(match_mask);

        if (tz_empty < tz_match) {
            i += tz_empty;

            self->metadata[i] = h2;
            self->entries[i] = (t4_stset_entry_t) {
                .hash = hash,
                .data = data,
                .size = data_size,
            };

            return true;
        }

        while (match_mask) {
            const t4_stset_entry_t * e = self->entries + i + tz_match;

            if (data_size == e->size && memcmp(data, e->data, data_size) == 0) {
                return false;
            }

            match_mask &= ~(1 << tz_match);
            tz_match = _tzcnt_u32(match_mask);

            if (tz_empty < tz_match) {
                i += tz_empty;

                self->metadata[i] = h2;
                self->entries[i] = (t4_stset_entry_t) {
                    .hash = hash,
                    .data = data,
                    .size = data_size,
                };

                return true;
            }
        }

        i = (i + t4_stset_alignment) % self->capacity;
        if (i == start) {
            t4_stset_increase_capacity_avx2(self);
            start = T4_ALIGN_DOWN(h1 % self->capacity, t4_stset_alignment);
            i = start;
        }
    }
}

static bool t4_stset_exists_avx2(const t4_stset_t * self, const void * data, const size_t data_size) {
    const u64 hash = t4_make_hash_h1h2(data, data_size);

    const __m256i r_h2 = _mm256_set1_epi8(T4_GET_H2(hash) | T4_FILLED);
    const __m256i r_empty = _mm256_set1_epi8(T4_FILLED);

    const u64 start = T4_ALIGN_DOWN(T4_GET_H1(hash) % self->capacity, t4_stset_alignment);
    u64 i = start;

    do {
        const __m256i candidates = _mm256_load_si256((const __m256i *)(self->metadata + i));

        const u32 empty_mask = _mm256_movemask_epi8(_mm256_andnot_si256(candidates, r_empty));
        u32 match_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(candidates, r_h2));

        const u32 tz_empty = _tzcnt_u32(empty_mask);
        u32 tz_match = _tzcnt_u32(match_mask);

        if (tz_empty < tz_match) {
            return false;
        }

        while (match_mask) {
            const t4_stset_entry_t * e = self->entries + i + tz_match;

            if (data_size == e->size && memcmp(data, e->data, data_size) == 0) {
                return true;
            }

            match_mask &= ~(1 << tz_match);
            tz_match = _tzcnt_u32(match_mask);

            if (tz_empty < tz_match) {
                return false;
            }
        }

        i = (i + t4_stset_alignment) % self->capacity;
    } while (i != start);

    return false;
}

/* Set end */

/* Init begin */

void t4_internal_stset_init(void) {
    const t4_cpu_features_t features = t4_get_cpu_features();

    if (features.avx2 && features.bmi1) {
        t4_internal_stset_vtable = (struct t4_internal_stset_vtable) {
            .get_alignment = t4_stset_get_alignment_impl,

            .new = t4_stset_new_aligned,
            .free = t4_stset_free_aligned,

            .insert_unchecked = t4_stset_insert_unchecked_avx2,
            .try_insert = t4_stset_try_insert_avx2,

            .exists = t4_stset_exists_avx2,
        };
        t4_stset_alignment = alignof(__m256i);

    } else {
        /* TODO Fallback implementation, probably steal that from qhm */
        assert(!"not implemented");

        t4_internal_stset_vtable = (struct t4_internal_stset_vtable) {
            .get_alignment = NULL,
            .new = NULL,
            .free = NULL,
            .insert_unchecked = NULL,
            .try_insert = NULL,
            .exists = NULL,
        };
        t4_stset_alignment = alignof(u8);
    }

    t4_stset_seed = time(NULL);
}

static t4_stset_t t4_internal_stset_new_with_init(const size_t capacity) {
    t4_internal_stset_init();
    return t4_internal_stset_vtable.new(capacity);
}

static size_t t4_internal_stset_get_alignment_with_init(void) {
    t4_internal_stset_init();
    return t4_stset_alignment;
}

struct t4_internal_stset_vtable t4_internal_stset_vtable = {
    .get_alignment = t4_internal_stset_get_alignment_with_init,
    .new = t4_internal_stset_new_with_init,
    .free = NULL,
    .insert_unchecked = NULL,
    .try_insert = NULL,
    .exists = NULL,
};

/* Init end */
