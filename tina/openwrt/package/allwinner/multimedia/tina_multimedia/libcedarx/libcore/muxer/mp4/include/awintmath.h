#ifndef _INTMATH_H
#define _INTMATH_H

#include <stdint.h>

#include "awconfig.h"
#include "awattributes.h"

#if HAVE_FAST_CLZ
#if AW_GCC_VERSION_AT_LEAST(3,4)
#ifndef aw_muxer_log2
#   define aw_muxer_log2(x) (31 - __builtin_clz((x)|1))
#   ifndef aw_muxer_log2_16bit
#      define aw_muxer_log2_16bit aw_muxer_log2
#   endif
#endif /* aw_muxer_log2 */
#endif /* AW_GCC_VERSION_AT_LEAST(3,4) */
#endif

static uint8_t aw_muxer_log2_tab[256]={
        0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};
#ifndef aw_muxer_log2
#define aw_muxer_log2 aw_muxer_log2_c
static aw_always_inline aw_const int aw_muxer_log2_c(unsigned int v)
{
    int n = 0;
    if (v & 0xffff0000) {
        v >>= 16;
        n += 16;
    }
    if (v & 0xff00) {
        v >>= 8;
        n += 8;
    }
    n += aw_muxer_log2_tab[v];

    return n;
}
#endif

#ifndef aw_muxer_log2_16bit
#define aw_muxer_log2_16bit aw_muxer_log2_16bit_c
static aw_always_inline aw_const int aw_muxer_log2_16bit_c(unsigned int v)
{
    int n = 0;
    if (v & 0xff00) {
        v >>= 8;
        n += 8;
    }
    n += aw_muxer_log2_tab[v];

    return n;
}
#endif

#define aw_log2       aw_muxer_log2
#define aw_log2_16bit aw_muxer_log2_16bit


/**
 * @addtogroup lavu_math
 * @{
 */

#if HAVE_FAST_CLZ
#if AW_GCC_VERSION_AT_LEAST(3,4)
#ifndef aw_ctz
#define aw_ctz(v) __builtin_ctz(v)
#endif
#ifndef aw_ctzll
#define aw_ctzll(v) __builtin_ctzll(v)
#endif
#ifndef aw_clz
#define aw_clz(v) __builtin_clz(v)
#endif
#endif
#endif

#ifndef aw_ctz
#define aw_ctz aw_ctz_c
/**
 * Trailing zero bit count.
 *
 * @param v  input value. If v is 0, the result is undefined.
 * @return   the number of trailing 0-bits
 */
/* We use the De-Bruijn method outlined in:
 * http://supertech.csail.mit.edu/papers/debruijn.pdf. */
static aw_always_inline aw_const int aw_ctz_c(int v)
{
    static const uint8_t debruijn_ctz32[32] = {
        0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
        31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
    };
    return debruijn_ctz32[(uint32_t)((v & -v) * 0x077CB531U) >> 27];
}
#endif

#ifndef aw_ctzll
#define aw_ctzll aw_ctzll_c
/* We use the De-Bruijn method outlined in:
 * http://supertech.csail.mit.edu/papers/debruijn.pdf. */
static aw_always_inline aw_const int aw_ctzll_c(long long v)
{
    static const uint8_t debruijn_ctz64[64] = {
        0, 1, 2, 53, 3, 7, 54, 27, 4, 38, 41, 8, 34, 55, 48, 28,
        62, 5, 39, 46, 44, 42, 22, 9, 24, 35, 59, 56, 49, 18, 29, 11,
        63, 52, 6, 26, 37, 40, 33, 47, 61, 45, 43, 21, 23, 58, 17, 10,
        51, 25, 36, 32, 60, 20, 57, 16, 50, 31, 19, 15, 30, 14, 13, 12
    };
    return debruijn_ctz64[(uint64_t)((v & -v) * 0x022FDD63CC95386DU) >> 58];
}
#endif

#ifndef aw_clz
#define aw_clz aw_clz_c
static aw_always_inline aw_const unsigned aw_clz_c(unsigned x)
{
    unsigned i = sizeof(x) * 8;

    while (x) {
        x >>= 1;
        i--;
    }

    return i;
}
#endif

#if AW_GCC_VERSION_AT_LEAST(3,4)
#ifndef aw_parity
#define aw_parity __builtin_parity
#endif
#endif

/**
 * @}
 */
#endif /* AWUTIL_INTMATH_H */
