#ifndef AWUTIL_MEM_INTERNAL_H
#define AWUTIL_MEM_INTERNAL_H

#include "awassert.h"
#include "awmem.h"

static inline int aw_fast_malloc(void *ptr, unsigned int *size, size_t min_size, int zero_realloc)
{
    void *val;

    memcpy(&val, ptr, sizeof(val));
    if (min_size <= *size) {
        aw_assert0(val || !min_size);
        return 0;
    }
    min_size = AWMAX(min_size + min_size / 16 + 32, min_size);
    mux_freep(ptr);
    val = zero_realloc ? mux_mallocz(min_size) : mux_malloc(min_size);
    memcpy(ptr, &val, sizeof(val));
    if (!val)
        min_size = 0;
    *size = min_size;
    return 1;
}
#endif /* AWUTIL_MEM_INTERNAL_H */
