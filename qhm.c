#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#include "MurmurHash3.h"

/* Enables checks inside the insertion functions which prevent duplicates. */
#define QHM_DEBUG_ASSERTIONS 0

/* utilities begin */

/* The seed for the hashmap, initialized in the beginning of main */
static uint32_t qhm_seed;

#define qhm_free(x) (free(x), *(&(x)) = NULL)

void * qhm_calloc(size_t size, size_t n)
{
    void * ptr = calloc(n, size);
    assert(ptr);
    return ptr;
}

typedef struct {
    char * buf;
    size_t size;
} qhm_rf_t;

qhm_rf_t read_file(const char * filepath)
{
    FILE * f = fopen(filepath, "rb");
    if (!f) {
        return (qhm_rf_t) { .buf = NULL, .size = 0, };
    }

    qhm_rf_t res;

    fseek(f, 0L, SEEK_END);
    res.size = ftell(f);
    fseek(f, 0L, SEEK_SET);

    res.buf = qhm_calloc(1, res.size);

    assert(fread(res.buf, 1, res.size, f) != 0);

    fclose(f);

    return res;
}

/* utilities end */

/* hm begin */

typedef struct ito_set_entry {
    const char * data;
    uint32_t data_len;

    /* This is stored in padding, so it's free, and it also helps by
       removing the need to recompute the hash on resize */
    uint32_t hash;

} qhm_entry_t;

typedef struct ito_set {
    qhm_entry_t * entries;
    uint32_t capacity;

} qhm_map_t;

static inline void qhm_map_init(qhm_map_t * m, uint32_t capacity)
{
    m->capacity = capacity;
    m->entries = qhm_calloc(sizeof(qhm_entry_t), capacity);
}

void qhm_map_free(qhm_map_t * m)
{
    qhm_free(m->entries);
    m->capacity = 0;
}

void qhm_map_increase_capacity(qhm_map_t * m)
{
    uint32_t new_capacity = m->capacity * 2;
    qhm_entry_t * new_entries = qhm_calloc(sizeof(qhm_entry_t), new_capacity);

    for (uint32_t i = 0; i < m->capacity; i++) {
        qhm_entry_t * curr = m->entries + i;

        if (!curr->data) {
            continue;
        }

        const uint32_t start = curr->hash % new_capacity;
        qhm_entry_t * entry;

        for (uint32_t j = 0; j < new_capacity; j++) {
            entry = new_entries + (start + j * j) % new_capacity;
            
            if (!entry->data) {
                break;
            }
        }

        memcpy(entry, curr, sizeof(qhm_entry_t));
    }

    free(m->entries);

    m->entries = new_entries;
    m->capacity = new_capacity;
}

void qhm_map_insert_unchecked(qhm_map_t * m, const char * data, uint32_t data_len)
{
    uint32_t hash;
    MurmurHash3_x86_32(data, data_len, qhm_seed, &hash);

rehash:;
    const uint32_t start = hash % m->capacity;
    qhm_entry_t * entry = m->entries + start;

    if (entry->data) {
#if QHM_DEBUG_ASSERTIONS
        if (entry->data_len == data_len) {
            assert(strncmp(data, entry->data, data_len) != 0);
        }
#endif

        for (uint32_t i = 1; i < m->capacity; i++) {
            entry = m->entries + ((start + i * i) % m->capacity);

            if (!entry->data) {
                break;
            }

#if QHM_DEBUG_ASSERTIONS
            if (entry->data_len == data_len) {
                assert(strncmp(data, entry->data, data_len) != 0);
            }
#endif

            if (i == start) {
                qhm_map_increase_capacity(m);
                goto rehash;
            }
        }
    }

    *entry = (qhm_entry_t) {
        .data = data,
        .data_len = data_len,
        .hash = hash,
    };
}

bool qhm_map_try_insert(qhm_map_t * m, const char * data, uint32_t data_len)
{
    uint32_t hash;
    MurmurHash3_x86_32(data, data_len, qhm_seed, &hash);

    /* This label is still used when QCM_LINEAR is set */
rehash:;
    uint32_t start = hash % m->capacity;

    qhm_entry_t * entry = m->entries + start;

    if (entry->data) {
        if (entry->data_len == data_len) {
            if (strncmp(data, entry->data, data_len) == 0) {
                return false;
            }
        }

        for (uint32_t i = 1; i < m->capacity; i++) {
            entry = m->entries + ((start + i * i) % m->capacity);

            if (!entry->data) {
                break;
            }

            if (entry->data_len == data_len && strncmp(data, entry->data, data_len) == 0) {
                return false;
            }

            if (i == start) {
                /* In the event of a capacity increase, it is guaranteed that there are no duplicates
                   so we can forego the comparisons. */
                qhm_map_increase_capacity(m);

                start = hash % m->capacity;

                entry = m->entries + start;

                /* The capacity has doubled here, so surely there is never going to be a need for another capacity increase here, right? */

                if (!entry->data) {
                    break;
                }

                for (uint32_t i = 1;; i++) {
                    entry = m->entries + ((start + i * i) % m->capacity);
                    if (!entry->data) {
                        goto capacity_increased;
                    }
                }
            }
        }
    }

capacity_increased:;

    *entry = (qhm_entry_t) {
        .data = data,
        .data_len = data_len,
        .hash = hash,
    };

    return true;
}

qhm_entry_t * qhm_map_get(const qhm_map_t * m, const char * data, uint32_t data_len)
{
    uint32_t hash;
    MurmurHash3_x86_32(data, data_len, qhm_seed, &hash);

    const uint32_t start = hash % m->capacity;

    qhm_entry_t * entry = m->entries + start;

    if (!entry->data) {
        return NULL;
    }
    
    if (entry->data_len == data_len && strncmp(data, entry->data, data_len) == 0) {
        return entry;
    }
    
    for (uint32_t i = 1; i < m->capacity; i++) {
        entry = m->entries + ((start + i * i) % m->capacity);

        if (!entry->data) {
            break;
        }

        if (entry->data_len != data_len) {
            continue;
        }

        if (strncmp(data, entry->data, data_len) == 0) {
            return entry;
        }
    }

    return NULL;
}

/* hm code end */

// FIXME this is completely broken lol
#define QHM_AVX2_TEST 0

#if QHM_AVX2_TEST

#include <stdalign.h>
#include <immintrin.h>

static size_t t4_alignment = alignof(__m256i);


typedef struct t4_data {
    char * ptr;
    char * aligned;
    size_t size;
} t4_data_t;

/* My solution to the input alignment issue was to make sure the input pointer is aligned manually and to reserve
 * some extra capacity to guarantee alignment. Looking at a more clever project now, they jump to the last index
 * in the buffer and align that, then reverse towards zero until it is no longer aligned and then they switch to
 * a slow implementation. I think I will stick with my approach, but that is a cool thing to note for the future.
 */
t4_data_t t4_read_file(const char * fp) {
    FILE * f = fopen(fp, "rb");
    if (f == NULL) {
        return (t4_data_t) { .ptr = NULL, .aligned = NULL, .size = 0 };
    }

    t4_data_t res;

    const size_t align_value = t4_alignment == 0 ? 0 : t4_alignment - 1;

    fseek(f, 0l, SEEK_END);
    const size_t size = ftell(f);
    fseek(f, 0l, SEEK_SET);

    res.size = (size + align_value) & ~align_value;

    // res.ptr = t4_calloc(res.size + align_value, sizeof(char));
    res.ptr = qhm_calloc(sizeof(char), res.size + align_value);

    res.aligned = (char *)(((uintptr_t)res.ptr + align_value) & ~align_value);

    const size_t read = fread(res.aligned, sizeof(char), res.size, f);
    assert(read != 0);

    return res;
}

void t4_process_avx2(qhm_map_t * in, const qhm_map_t * eng, char * data, const size_t data_size) {
    assert((uintptr_t)data % t4_alignment == 0);
    assert(data_size % t4_alignment == 0);

    uint32_t length = 0;
    size_t offset = 0;

    const __m256i capital_b1 = _mm256_set1_epi8('A' - 1);
    const __m256i capital_b2 = _mm256_set1_epi8('Z' + 1);

    const __m256i lower_b1 = _mm256_set1_epi8('a' - 1);
    const __m256i lower_b2 = _mm256_set1_epi8('z' + 1);

    const __m256i difference = _mm256_set1_epi8('a' - 'A');

    for (size_t chunk = 0; chunk < data_size; chunk += t4_alignment) {
        char * ptr = data + chunk;

        __m256i curr = _mm256_load_si256((const __m256i *)ptr);

        /* Make characters lowercase */

        __m256i matched = _mm256_and_si256(
            _mm256_cmpgt_epi8(curr, capital_b1),
            _mm256_cmpgt_epi8(capital_b2, curr));

        const __m256i tolower = _mm256_and_si256(matched, difference);
        curr = _mm256_add_epi8(curr, tolower);

        /* NOTE: I'm not sure whether this is good for performance. Streaming also exists, but
         * do I use the data too early for that to be usable?
         */
        _mm256_store_si256((__m256i *)ptr, curr);

        /* Get all english letters */

        matched = _mm256_and_si256(
            _mm256_cmpgt_epi8(curr, lower_b1),
            _mm256_cmpgt_epi8(lower_b2, curr));

        uint32_t mask = _mm256_movemask_epi8(matched);

        /* Process */

        /* TODO bench this against a normal implementation */

        /* If mask has zero bits near the end, without this the offset will become incorrect. */
        size_t skip = 32;

        do {
            if (mask & 1) {
                length += 1;
            } else {
                if (length != 0) {
                    // printf("%.*s\n", length, data + offset);
                    const char * w = data + offset;
                    
                    if (qhm_map_try_insert(in, w, length)) {
                        qhm_entry_t * e = qhm_map_get(eng, w, length);

                        if (!e) {
                            printf("%.*s\n", length, w);
                        }
                    }
                }
                offset += length + 1;
                length = 0;
            }

            mask >>= 1;
            skip -= 1;
        } while (mask);

        offset += skip;
    }

    if (length != 0) {
        // printf("%.*s\n", length, data + offset);
        const char * w = data + offset;
            
        if (qhm_map_try_insert(in, w, length)) {
            qhm_entry_t * e = qhm_map_get(eng, w, length);

            if (!e) {
                printf("%.*s\n", length, w);
            }
        }
    }
}

#endif

int main(int argc, const char * argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [file]\n", argv[0]);
        return 1;
    }

#if !QHM_AVX2_TEST

    qhm_rf_t input_file = read_file(argv[1]);
    if (!input_file.buf) {
        fprintf(stderr, "Failed to read '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

#else

    t4_data_t d = t4_read_file(argv[1]);
    if (!d.ptr) {
        fprintf(stderr, "Failed to read '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

#endif

    qhm_seed = time(NULL);

    qhm_rf_t engd_file = read_file("sorted.bin");
    assert(engd_file.buf);

    // TODO figure out a sensible way to get an initial capacity rather than hardcoding.
    // Same goes for the capacity of engd down below, though that one should be rounded up to the next power of 2.
    uint32_t eng_ordered_cap = 200000;
    uint32_t eng_ordered_len = 0;
    const char ** eng_ordered = qhm_calloc(sizeof(const char *), eng_ordered_cap);

    qhm_map_t engd;

    // Initial capacity of 262144. Powers of 2 appeared to be faster in my benchmarks.
    qhm_map_init(&engd, 2 << 18);

    {
        const char * start = engd_file.buf;
        for (size_t i = 0; i < engd_file.size; i++) {
            const char * c = engd_file.buf + i;

            if (*c == '\0') {
                uint32_t length = c - start;

                eng_ordered[eng_ordered_len] = start;
                eng_ordered_len += 1;
                if (eng_ordered_cap <= eng_ordered_len) {
                    eng_ordered_cap *= 2;
                    eng_ordered = realloc(eng_ordered, eng_ordered_cap * sizeof(const char *));
                    assert(eng_ordered);
                }

                qhm_map_insert_unchecked(&engd, start, length);

                start = c + 1;
            }
        }
    }

    /* application logic begin */

    qhm_map_t input;
    qhm_map_init(&input, 10000);

#if !QHM_AVX2_TEST

    uint32_t non_english_words = 0;
    uint32_t unique_words = 0;
    uint32_t total_words = 0;
    
    {
        char * start = input_file.buf;
        for (size_t i = 0; i < input_file.size; i++) {
            char * c = input_file.buf + i;

            if (!isalpha(*c)) {
                uint32_t length = c - start;

                if (length != 0) {
                    bool is_new = qhm_map_try_insert(&input, start, length);
                    
                    unique_words += is_new;
                    total_words += 1;

                    if (is_new) {
                        qhm_entry_t * entry = qhm_map_get(&engd, start, length);
                        if (!entry) {
                            non_english_words += 1;
                            printf("%.*s\n", length, start);
                        }
                    }
                }

                start = c + 1;

            } else {
                *c = (char)tolower(*c);
            }
        }
    }

    printf("\nTotal words: %u\n", total_words);
    printf("Unique words: %u\n", unique_words);
    printf("Non-english words: %u\n", non_english_words);

#else

    t4_process_avx2(&input, &engd, d.aligned, d.size);

#endif

    /* application logic end */

    qhm_map_free(&input);

#if !QHM_AVX2_TEST
    qhm_free(input_file.buf);
#else
    qhm_free(d.ptr);
#endif

    qhm_map_free(&engd);

    qhm_free(engd_file.buf);
    qhm_free(eng_ordered);

    return 0;
}
