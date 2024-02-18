#ifndef AWUTIL_BSWAP_H
#define AWUTIL_BSWAP_H

#include <stdint.h>
#include "awconfig.h"
#include "awattributes.h"

#ifdef HAVE_AW_CONFIG_H

#include "config.h"

#if   ARCH_AARCH64
#   include "aarch64/bswap.h"
#elif ARCH_ARM
#   include "arm/bswap.h"
#elif ARCH_AWR32
#   include "avr32/bswap.h"
#elif ARCH_SH4
#   include "sh4/bswap.h"
#elif ARCH_X86
#   include "x86/bswap.h"
#endif

#endif /* HAVE_AW_CONFIG_H */

#define AW_BSWAP16C(x) (((x) << 8 & 0xff00)  | ((x) >> 8 & 0x00ff))
#define AW_BSWAP32C(x) (AW_BSWAP16C(x) << 16 | AW_BSWAP16C((x) >> 16))
#define AW_BSWAP64C(x) (AW_BSWAP32C(x) << 32 | AW_BSWAP32C((x) >> 32))

#define AW_BSWAPC(s, x) AW_BSWAP##s##C(x)

#ifndef aw_bswap16
static aw_always_inline aw_const uint16_t aw_bswap16(uint16_t x)
{
    x= (x>>8) | (x<<8);
    return x;
}
#endif

#ifndef aw_bswap32
static aw_always_inline aw_const uint32_t aw_bswap32(uint32_t x)
{
    return AW_BSWAP32C(x);
}
#endif

#ifndef aw_bswap64
static inline uint64_t aw_const aw_bswap64(uint64_t x)
{
    return (uint64_t)aw_bswap32(x) << 32 | aw_bswap32(x >> 32);
}
#endif

// be2ne ... big-endian to native-endian
// le2ne ... little-endian to native-endian

#if AW_HAVE_BIGENDIAN
#define aw_be2ne16(x) (x)
#define aw_be2ne32(x) (x)
#define aw_be2ne64(x) (x)
#define aw_le2ne16(x) aw_bswap16(x)
#define aw_le2ne32(x) aw_bswap32(x)
#define aw_le2ne64(x) aw_bswap64(x)
#define AW_BE2NEC(s, x) (x)
#define AW_LE2NEC(s, x) AW_BSWAPC(s, x)
#else
#define aw_be2ne16(x) aw_bswap16(x)
#define aw_be2ne32(x) aw_bswap32(x)
#define aw_be2ne64(x) aw_bswap64(x)
#define aw_le2ne16(x) (x)
#define aw_le2ne32(x) (x)
#define aw_le2ne64(x) (x)
#define AW_BE2NEC(s, x) AW_BSWAPC(s, x)
#define AW_LE2NEC(s, x) (x)
#endif

#define AW_BE2NE16C(x) AW_BE2NEC(16, x)
#define AW_BE2NE32C(x) AW_BE2NEC(32, x)
#define AW_BE2NE64C(x) AW_BE2NEC(64, x)
#define AW_LE2NE16C(x) AW_LE2NEC(16, x)
#define AW_LE2NE32C(x) AW_LE2NEC(32, x)
#define AW_LE2NE64C(x) AW_LE2NEC(64, x)

#endif /* AVUTIL_BSWAP_H */
