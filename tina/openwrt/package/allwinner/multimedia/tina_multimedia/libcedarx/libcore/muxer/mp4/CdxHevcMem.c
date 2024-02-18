#define _XOPEN_SOURCE 600

#include "awconfig.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "awassert.h"
#include "awcommon.h"
#include "awintreadwrite.h"
#include "awmem.h"

#define AW_MEMORY_POISON 0x2a
#define CONFIG_MEMORY_POISONING 0
#define AWERROR(e) (-(e))
#ifdef MALLOC_PREFIX

#define malloc         AW_JOIN(MALLOC_PREFIX, malloc)
#define memalign       AW_JOIN(MALLOC_PREFIX, memalign)
#define posix_memalign AW_JOIN(MALLOC_PREFIX, posix_memalign)
#define realloc        AW_JOIN(MALLOC_PREFIX, realloc)
#define free           AW_JOIN(MALLOC_PREFIX, free)

void *malloc(size_t size);
void *memalign(size_t align, size_t size);
int   posix_memalign(void **ptr, size_t align, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);

#endif /* MALLOC_PREFIX */

#include "awmem_internal.h"

#define ALIGN (HAVE_AVX ? 32 : 16)

/* NOTE: if you want to override these functions with your own
 * implementations (not recommended) you have to link libav* as
 * dynamic libraries and remove -Wl,-Bsymbolic from the linker flags.
 * Note that this will cost performance. */

static size_t max_alloc_size= INT_MAX;

void mux_max_alloc(size_t max){
    max_alloc_size = max;
}

void *mux_malloc(size_t size)
{
    void *ptr = NULL;

    /* let's disallow possibly ambiguous cases */
    if (size > (max_alloc_size - 32))
        return NULL;

#if HAVE_POSIX_MEMALIGN
    if (size) //OS X on SDK 10.6 has a broken posix_memalign implementation
    {
        //if (posix_memalign(&ptr, ALIGN, size))
        //    ptr = NULL;
        ptr = malloc(size);
    }
#elif HAVE_ALIGNED_MALLOC
    ptr = _aligned_malloc(size, ALIGN);
#elif HAVE_MEMALIGN
#ifndef __DJGPP__
    ptr = memalign(ALIGN, size);
#else
    ptr = memalign(size, ALIGN);
#endif
    /* Why 64?
     * Indeed, we should align it:
     *   on  4 for 386
     *   on 16 for 486
     *   on 32 for 586, PPro - K6-III
     *   on 64 for K7 (maybe for P3 too).
     * Because L1 and L2 caches are aligned on those values.
     * But I don't want to code such logic here!
     */
    /* Why 32?
     * For AVX ASM. SSE / NEON needs only 16.
     * Why not larger? Because I did not see a difference in benchmarks ...
     */
    /* benchmarks with P3
     * memalign(64) + 1          3071, 3051, 3032
     * memalign(64) + 2          3051, 3032, 3041
     * memalign(64) + 4          2911, 2896, 2915
     * memalign(64) + 8          2545, 2554, 2550
     * memalign(64) + 16         2543, 2572, 2563
     * memalign(64) + 32         2546, 2545, 2571
     * memalign(64) + 64         2570, 2533, 2558
     *
     * BTW, malloc seems to do 8-byte alignment by default here.
     */
#else
    ptr = malloc(size);
#endif
    if(!ptr && !size) {
        size = 1;
        ptr= mux_malloc(size);
    }
#if CONFIG_MEMORY_POISONING
    if (ptr)
        memset(ptr, AW_MEMORY_POISON, size);
#endif
    return ptr;
}

void *mux_realloc(void *ptr, size_t size)
{
    /* let's disallow possibly ambiguous cases */
    if (size > (max_alloc_size - 32))
        return NULL;

#if HAVE_ALIGNED_MALLOC
    return _aligned_realloc(ptr, size + !size, ALIGN);
#else
    return realloc(ptr, size + !size);
#endif
}

void *mux_realloc_f(void *ptr, size_t nelem, size_t elsize)
{
    size_t size;
    void *r;

    if (mux_size_mult(elsize, nelem, &size)) {
        mux_free(ptr);
        return NULL;
    }
    r = mux_realloc(ptr, size);
    if (!r)
        mux_free(ptr);
    return r;
}

int mux_reallocp(void *ptr, size_t size)
{
    void *val;

    if (!size) {
        mux_freep(ptr);
        return 0;
    }

    memcpy(&val, ptr, sizeof(val));
    val = mux_realloc(val, size);

    if (!val) {
        mux_freep(ptr);
        return AWERROR(ENOMEM);
    }

    memcpy(ptr, &val, sizeof(val));
    return 0;
}

void *mux_realloc_array(void *ptr, size_t nmemb, size_t size)
{
    if (!size || nmemb >= INT_MAX / size)
        return NULL;
    return mux_realloc(ptr, nmemb * size);
}

int mux_reallocp_array(void *ptr, size_t nmemb, size_t size)
{
    void *val;

    memcpy(&val, ptr, sizeof(val));
    val = mux_realloc_f(val, nmemb, size);
    memcpy(ptr, &val, sizeof(val));
    if (!val && nmemb && size)
        return AWERROR(ENOMEM);

    return 0;
}

void mux_free(void *ptr)
{
#if HAVE_ALIGNED_MALLOC
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void mux_freep(void *arg)
{
    void *val;

    memcpy(&val, arg, sizeof(val));
    memcpy(arg, &(void *){ NULL }, sizeof(val));
    mux_free(val);
}

void *mux_mallocz(size_t size)
{
    void *ptr = mux_malloc(size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}

void *mux_calloc(size_t nmemb, size_t size)
{
    if (size <= 0 || nmemb >= INT_MAX / size)
        return NULL;
    return mux_mallocz(nmemb * size);
}

char *mux_strdup(const char *s)
{
    char *ptr = NULL;
    if (s) {
        size_t len = strlen(s) + 1;
        ptr = mux_realloc(NULL, len);
        if (ptr)
            memcpy(ptr, s, len);
    }
    return ptr;
}

char *mux_strndup(const char *s, size_t len)
{
    char *ret = NULL, *end;

    if (!s)
        return NULL;

    end = memchr(s, 0, len);
    if (end)
        len = end - s;

    ret = mux_realloc(NULL, len + 1);
    if (!ret)
        return NULL;

    memcpy(ret, s, len);
    ret[len] = 0;
    return ret;
}

void *mux_memdup(const void *p, size_t size)
{
    void *ptr = NULL;
    if (p) {
        ptr = mux_malloc(size);
        if (ptr)
            memcpy(ptr, p, size);
    }
    return ptr;
}
static void fill16(uint8_t *dst, int len)
{
    uint32_t v = AW_RN16(dst - 2);

    v |= v << 16;

    while (len >= 4) {
        AW_WN32(dst, v);
        dst += 4;
        len -= 4;
    }

    while (len--) {
        *dst = dst[-2];
        dst++;
    }
}

static void fill24(uint8_t *dst, int len)
{
#if HAVE_BIGENDIAN
    uint32_t v = AW_RB24(dst - 3);
    uint32_t a = v << 8  | v >> 16;
    uint32_t b = v << 16 | v >> 8;
    uint32_t c = v << 24 | v;
#else
    uint32_t v = AW_RL24(dst - 3);
    uint32_t a = v       | v << 24;
    uint32_t b = v >> 8  | v << 16;
    uint32_t c = v >> 16 | v << 8;
#endif

    while (len >= 12) {
        AW_WN32(dst,     a);
        AW_WN32(dst + 4, b);
        AW_WN32(dst + 8, c);
        dst += 12;
        len -= 12;
    }

    if (len >= 4) {
        AW_WN32(dst, a);
        dst += 4;
        len -= 4;
    }

    if (len >= 4) {
        AW_WN32(dst, b);
        dst += 4;
        len -= 4;
    }

    while (len--) {
        *dst = dst[-3];
        dst++;
    }
}

static void fill32(uint8_t *dst, int len)
{
    uint32_t v = AW_RN32(dst - 4);

    while (len >= 4) {
        AW_WN32(dst, v);
        dst += 4;
        len -= 4;
    }

    while (len--) {
        *dst = dst[-4];
        dst++;
    }
}

void mux_memcpy_backptr(uint8_t *dst, int back, int cnt)
{
    const uint8_t *src = &dst[-back];
    if (!back)
        return;

    if (back == 1) {
        memset(dst, *src, cnt);
    } else if (back == 2) {
        fill16(dst, cnt);
    } else if (back == 3) {
        fill24(dst, cnt);
    } else if (back == 4) {
        fill32(dst, cnt);
    } else {
        if (cnt >= 16) {
            int blocklen = back;
            while (cnt > blocklen) {
                memcpy(dst, src, blocklen);
                dst       += blocklen;
                cnt       -= blocklen;
                blocklen <<= 1;
            }
            memcpy(dst, src, cnt);
            return;
        }
        if (cnt >= 8) {
            AW_COPY32U(dst,     src);
            AW_COPY32U(dst + 4, src + 4);
            src += 8;
            dst += 8;
            cnt -= 8;
        }
        if (cnt >= 4) {
            AW_COPY32U(dst, src);
            src += 4;
            dst += 4;
            cnt -= 4;
        }
        if (cnt >= 2) {
            AW_COPY16U(dst, src);
            src += 2;
            dst += 2;
            cnt -= 2;
        }
        if (cnt)
            *dst = *src;
    }
}

void *mux_fast_realloc(void *ptr, unsigned int *size, size_t min_size)
{
    if (min_size < *size)
        return ptr;

    min_size = AWMAX(min_size + min_size / 16 + 32, min_size);

    ptr = mux_realloc(ptr, min_size);
    /* we could set this to the unmodified min_size but this is safer
     * if the user lost the ptr and uses NULL now
     */
    if (!ptr)
        min_size = 0;

    *size = min_size;

    return ptr;
}

void mux_fast_malloc(void *ptr, unsigned int *size, size_t min_size)
{
    aw_fast_malloc(ptr, size, min_size, 0);
}

void mux_fast_mallocz(void *ptr, unsigned int *size, size_t min_size)
{
    aw_fast_malloc(ptr, size, min_size, 1);
}
