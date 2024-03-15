#include "t4/common.h"
#include "t4/stset.h"
#include "t4/mem.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char * buf;
    size_t size;
} t4_filebuf_t;

static t4_filebuf_t t4_read_file(const char * fp, const size_t alignment) {
    FILE * f = fopen(fp, "rb");
    if (f == NULL) {
        return (t4_filebuf_t) { .buf = NULL, .size = 0, };
    }

    t4_filebuf_t res;

    fseek(f, 0l, SEEK_END);
    res.size = ftell(f);
    fseek(f, 0l, SEEK_SET);

    const size_t align = alignment - 1;

    res.size = T4_ALIGN_UP(res.size, align);

    res.buf = t4_calloc_aligned(res.size, alignment);

    const size_t read = fread(res.buf, 1, res.size, f);
    assert(read != 0);

    fclose(f);

    return res;
}

int main(const int argc, const char * argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
        return 1;
    }

    const size_t alignment = t4_stset_get_alignment();

    t4_filebuf_t f = t4_read_file(argv[1], alignment);
    if (f.buf == NULL) {
        fprintf(stderr, "Failed to open %s: %s", argv[1], strerror(errno));
        return 1;
    }

    // This doesn't really need any alignment.
    t4_filebuf_t ef = t4_read_file("./sorted.bin", alignment);
    assert(ef.buf != NULL);

    // Simply using 200k instead of 350k (no rehash or size increase) will be significantly faster
    t4_stset_t eng = t4_stset_new(400000);

    {
        char * start = ef.buf;
        for (size_t i = 0; i < ef.size; i++) {
            char * c = ef.buf + i;
            if (*c == '\0') {
                const u64 length = c - start;
                if (length != 0) {
                    t4_stset_insert_unchecked(&eng, start, length);
                }
                start = c + 1;
            }
        }
    }

    // TODO decide at runtime based on the size of the input file
    t4_stset_t in = t4_stset_new(10000);

    u64 non_english = 0;
    u64 num_unique = 0;
    u64 num_total = 0;

    char * start = f.buf;
    for (size_t i = 0; i < f.size; i++) {
        char * c = f.buf + i;
        if (!isalpha(*c)) {
            const u64 length = c - start;

            if (length != 0) {
                num_total += 1;
                const bool unique = t4_stset_try_insert(&in, start, length);

                if (unique) {
                    num_unique += 1;
                    if (!t4_stset_exists(&eng, start, length)) {
                        non_english += 1;
                        printf("%.*s\n", (u32)length, start);
                    }
                }
            }

            start = c + 1;
        } else {
            *c = tolower(*c);
        }
    }

    printf("\nTotal words: %lu\n", num_total);
    printf("Unique words: %lu\n", num_unique);
    printf("Number of non-english words: %lu\n", non_english);

    t4_stset_free(&in);
    t4_stset_free(&eng);

    t4_free_aligned(f.buf);
    t4_free_aligned(ef.buf);

    return 0;
}
