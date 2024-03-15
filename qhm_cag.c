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

#if 0
// #include "MurmurHash3.h"
#else
#include "include/t4/wyhash.h"
#endif

#if 0

typedef struct {
    uint64_t accesses;
    uint64_t probes; 
} qhm_metric_t;

qhm_metric_t read_metrics;
qhm_metric_t write_uc_metrics;
qhm_metric_t write_try_metrics;

#endif

#define QHM_METRICS 0

/* Enables checks inside the insertion functions which prevent duplicates. */
#define QHM_DEBUG_ASSERTIONS 0

/* This is just to test the performance of linear and quadratic probing.
   As one might guess, quadratic wins. */
#define QHM_LINEAR 0

/* utilities begin */

/* The seed for the hashmap, initialized in the beginning of main */
static uint64_t qhm_seed;

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

typedef struct qhm_entry {
    const char * data;
    uint64_t data_len;
    uint64_t hash;

} qhm_entry_t;

typedef struct qhm_map {
    qhm_entry_t * entries;
    size_t capacity_idx;
    size_t capacity;

} qhm_map_t;

// source: https://planetmath.org/goodhashtableprimes
size_t primes[] = {
    53,97,193,389,769,1543,3079,6151,12289,24593,49157,98317,196613,393241,786433,1572869,3145739,6291469,12582917,25165843,50331653,100663319,201326611,402653189
};

static inline void qhm_map_init(qhm_map_t * m, size_t capacity_idx)
{
    m->capacity_idx = capacity_idx;
    m->capacity = primes[capacity_idx];
    m->entries = qhm_calloc(sizeof(qhm_entry_t), primes[capacity_idx]);
}

void qhm_map_free(qhm_map_t * m)
{
    qhm_free(m->entries);
    m->capacity = 0;
}

void qhm_map_increase_capacity(qhm_map_t * m)
{
    ++m->capacity_idx;
    size_t new_capacity = primes[m->capacity_idx];

    qhm_entry_t * new_entries = qhm_calloc(sizeof(qhm_entry_t), new_capacity);

    for (uint64_t i = 0; i < m->capacity; i++) {
        qhm_entry_t * curr = m->entries + i;

        if (!curr->data) continue;

        const uint64_t start = curr->hash % new_capacity;
        qhm_entry_t * entry;

#if !QHM_LINEAR

        for (uint64_t j = 0; j < new_capacity; j++) {
            entry = new_entries + (start + j * j) % new_capacity;
            
            if (!entry->data) {
                break;
            }
        }
#else
        uint64_t j = start;

        while (new_entries[j].data) {
            j = (j + 1) % new_capacity;
        }

        entry = new_entries + j;

#endif /* !QHM_LINEAR */

        memcpy(entry, curr, sizeof(qhm_entry_t));
    }

    free(m->entries);

    m->entries = new_entries;
    m->capacity = new_capacity;
}

void qhm_map_insert_unchecked(qhm_map_t * m, const char * data, uint64_t data_len)
{
#if QHM_METRICS
    write_uc_metrics.accesses += 1;
#endif

#if 0
    uint64_t hash;
    MurmurHash3_x86_32(data, data_len, qhm_seed, &hash);
#else
    uint64_t hash = wyhash(data, data_len, qhm_seed, _wyp);
#endif


rehash:;
    const uint64_t start = hash % m->capacity;
    qhm_entry_t * entry = m->entries + start;

#if !QHM_LINEAR

    if (entry->data) {
#if QHM_DEBUG_ASSERTIONS
        if (entry->data_len == data_len) {
            assert(strncmp(data, entry->data, data_len) != 0);
        }
#endif

        for (uint64_t i = 1; i < m->capacity; i++) {
#if QHM_METRICS
            write_uc_metrics.probes += 1;
#endif
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

#else

    uint64_t i = start;

    while (m->entries[i].data) {
        write_uc_metrics.probes += 1;
        i = (i + 1) % m->capacity;
        
        if (i == start) {
            qhm_map_increase_capacity(m);
            goto rehash;
        }
    }

    entry = m->entries + i;

#endif /* !QHM_LINEAR */

    *entry = (qhm_entry_t) {
        .data = data,
        .data_len = data_len,
        .hash = hash,
    };
}

bool qhm_map_try_insert(qhm_map_t * m, const char * data, uint64_t data_len)
{
#if 0

    uint64_t hash;
    MurmurHash3_x86_32(data, data_len, qhm_seed, &hash);
#else
    uint64_t hash = wyhash(data, data_len, qhm_seed, _wyp);
#endif
#if QHM_METRICS
    write_try_metrics.accesses += 1;
#endif

    /* This label is still used when QCM_LINEAR is set */
rehash:;
    uint64_t start = hash % m->capacity;

    qhm_entry_t * entry = m->entries + start;

#if !QHM_LINEAR

    if (entry->data) {
        if (entry->data_len == data_len) {
            if (strncmp(data, entry->data, data_len) == 0) {
                return false;
            }
        }

        for (uint64_t i = 1; i < m->capacity; i++) {
#if QHM_METRICS
            write_try_metrics.probes += 1;
#endif
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

                if (!entry->data) {
                    break;
                }

                for (uint64_t i = 1;; i++) {
                    entry = m->entries + ((start + i * i) % m->capacity);
                    if (!entry->data) {
                        goto capacity_increased;
                    }
                }
            }
        }
    }

#else

    uint64_t i = start;

    while (m->entries[i].data_len) {
        write_try_metrics.probes += 1;
        if (entry->data_len == data_len && strncmp(data, entry->data, data_len) == 0) {
            return false;
        }

        i = (i + 1) % m->capacity;

        entry = m->entries + i;

        if (i == start) {
            qhm_map_increase_capacity(m);
            goto rehash;
        }
    }

#endif /* !QHM_LINEAR */

capacity_increased:;

    *entry = (qhm_entry_t) {
        .data = data,
        .data_len = data_len,
        .hash = hash,
    };

    return true;
}

qhm_entry_t * qhm_map_get(const qhm_map_t * m, const char * data, uint64_t data_len)
{
#if 0
    uint64_t hash;
    MurmurHash3_x86_32(data, data_len, qhm_seed, &hash);
#else
    uint64_t hash = wyhash(data, data_len, qhm_seed, _wyp);
#endif

    const uint64_t start = hash % m->capacity;

    qhm_entry_t * entry = m->entries + start;

#if QHM_METRICS
    read_metrics.accesses += 1;
#endif

#if !QHM_LINEAR

    if (!entry->data) {
        return NULL;
    }
    
    if (entry->data_len == data_len && strncmp(data, entry->data, data_len) == 0) {
        return entry;
    }
    
    for (uint64_t i = 1; i < m->capacity; i++) {
#if QHM_METRICS
        read_metrics.probes += 1;
#endif
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

#else

    uint64_t i = start;

    while (m->entries[i].data) {
        read_metrics.probes += 1;
        entry = m->entries + i;

        if (data_len == entry->data_len && strncmp(data, entry->data, data_len) == 0) {
            return entry;
        }

        i = (i + 1) % m->capacity;

        if (i == start) {
            break;
        }
    }

#endif

    return NULL;
}

#if QHM_METRICS
void qhm_print_metrics(qhm_metric_t *metrics, const char *name) {
    double ratio = (double)metrics->probes / metrics->accesses;
    printf("%s: %d probes, %d accesses, %f\n", name, metrics->probes, metrics->accesses, ratio);
}
#endif

/* hm code end */

int main(int argc, const char * argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [file]\n", argv[0]);
        return 1;
    }

    qhm_rf_t input_file = read_file(argv[1]);
    if (!input_file.buf) {
        fprintf(stderr, "Failed to read '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    qhm_seed = time(NULL);

    qhm_rf_t engd_file = read_file("sorted.bin");
    assert(engd_file.buf);

#if 0
    size_t eng_ordered_cap = 200000;
    size_t eng_ordered_len = 0;
    const char ** eng_ordered = qhm_calloc(sizeof(const char *), eng_ordered_cap);
#endif

    qhm_map_t engd;

    // Initial capacity of 262144. Powers of 2 appeared to be faster in my benchmarks.
    qhm_map_init(&engd, 13);

    {
        const char * start = engd_file.buf;
        for (size_t i = 0; i < engd_file.size; i++) {
            const char * c = engd_file.buf + i;

            if (*c == '\0') {
                size_t length = c - start;

#if 0
                eng_ordered[eng_ordered_len] = start;
                eng_ordered_len += 1;
                if (eng_ordered_cap <= eng_ordered_len) {
                    eng_ordered_cap *= 2;
                    eng_ordered = realloc(eng_ordered, eng_ordered_cap * sizeof(const char *));
                    assert(eng_ordered);
                }
#endif

                qhm_map_insert_unchecked(&engd, start, length);

                start = c + 1;
            }
        }
    }

    /* application logic begin */

    qhm_map_t input;

    qhm_map_init(&input, 7);  // 7 -> 6151

    uint64_t non_english_words = 0;
    uint64_t unique_words = 0;
    uint64_t total_words = 0;

    // printf("begin reading input\n");
    
    {
        char * start = input_file.buf;
        for (size_t i = 0; i < input_file.size; i++) {
            char * c = input_file.buf + i;

            if (!isalpha(*c)) {
                uint64_t length = c - start;

                if (length != 0) {
                    bool is_new = qhm_map_try_insert(&input, start, length);
                    
                    unique_words += is_new;
                    total_words += 1;

                    if (is_new) {
                        qhm_entry_t * entry = qhm_map_get(&engd, start, length);
                        if (!entry) {
                            non_english_words += 1;
                            printf("%.*s\n", (uint32_t)length, start);
                        }
                    }
                }

                start = c + 1;

            } else {
                *c = (char)tolower(*c);
            }
        }
    }

    // printf("QHM_LINEAR: %d", QHM_LINEAR);
    printf("\nTotal words: %lu\n", total_words);
    printf("Unique words: %lu\n", unique_words);
    printf("Number of non-english words: %lu\n", non_english_words);

#if QHM_METRICS

    qhm_print_metrics(&read_metrics, "READ ENG DICT");
    qhm_print_metrics(&write_uc_metrics, "WRITE ENG DICT");
    qhm_print_metrics(&write_try_metrics, "WRITE INPUT");
    
#endif
    /* application logic end */

    qhm_map_free(&input);
    qhm_free(input_file.buf);

    qhm_map_free(&engd);
    qhm_free(engd_file.buf);

#if 0
    qhm_free(eng_ordered);
#endif

    return 0;
}