// TODO I wonder if include guards are necessary for this file
#ifndef T4_STSET_VTABLE_H_
#define T4_STSET_VTABLE_H_

#ifndef T4_STSET_H_
#   error "Do not include this file directly."
#endif

#include "t4/common.h"

typedef struct t4_stset t4_stset_t;

struct t4_internal_stset_vtable {
    size_t (*get_alignment)(void);

    t4_stset_t (*new)(size_t);
    void (*free)(t4_stset_t *);

    void (*insert_unchecked)(t4_stset_t *, void *, size_t);
    bool (*try_insert)(t4_stset_t *, void *, size_t);

    bool (*exists)(const t4_stset_t *, const void *, size_t);
};

extern struct t4_internal_stset_vtable t4_internal_stset_vtable;

extern void t4_internal_stset_init(void);

#endif /* T4_STSET_VTABLE_H_ */