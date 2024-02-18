#ifndef AWUTIL_INTREADWRITE_H
#define AWUTIL_INTREADWRITE_H

#include <stdint.h>
#include "awconfig.h"
#include "awattributes.h"
#include "awbswap.h"

typedef union {
    uint64_t u64;
    uint32_t u32[2];
    uint16_t u16[4];
    uint8_t  u8 [8];
    double   f64;
    float    f32[2];
} aw_alias aw_alias64;

typedef union {
    uint32_t u32;
    uint16_t u16[2];
    uint8_t  u8 [4];
    float    f32;
} aw_alias aw_alias32;

typedef union {
    uint16_t u16;
    uint8_t  u8 [2];
} aw_alias aw_alias16;

/*
 * Arch-specific headers can provide any combination of
 * AW_[RW][BLN](16|24|32|48|64) and AW_(COPY|SWAP|ZERO)(64|128) macros.
 * Preprocessor symbols must be defined, even if these are implemented
 * as inline functions.
 *
 * R/W means read/write, B/L/N means big/little/native endianness.
 * The following macros require aligned access, compared to their
 * unaligned variants: AW_(COPY|SWAP|ZERO)(64|128), AW_[RW]N[8-64]A.
 * Incorrect usage may range from abysmal performance to crash
 * depending on the platform.
 *
 * The unaligned variants are AW_[RW][BLN][8-64] and AW_COPY*U.
 */

#ifdef HAVE_AW_CONFIG_H

#include "awconfig.h"

#if   ARCH_ARM
#   include "arm/intreadwrite.h"
#elif ARCH_AVR32
#   include "avr32/intreadwrite.h"
#elif ARCH_MIPS
#   include "mips/intreadwrite.h"
#elif ARCH_PPC
#   include "ppc/intreadwrite.h"
#elif ARCH_TOMI
#   include "tomi/intreadwrite.h"
#elif ARCH_X86
#   include "x86/intreadwrite.h"
#endif

#endif /* HAVE_AW_CONFIG_H */

/*
 * Map AW_RNXX <-> AW_R[BL]XX for all variants provided by per-arch headers.
 */

#if AW_HAVE_BIGENDIAN

#   if    defined(AW_RN16) && !defined(AW_RB16)
#       define AW_RB16(p) AW_RN16(p)
#   elif !defined(AW_RN16) &&  defined(AW_RB16)
#       define AW_RN16(p) AW_RB16(p)
#   endif

#   if    defined(AW_WN16) && !defined(AW_WB16)
#       define AW_WB16(p, v) AW_WN16(p, v)
#   elif !defined(AW_WN16) &&  defined(AW_WB16)
#       define AW_WN16(p, v) AW_WB16(p, v)
#   endif

#   if    defined(AW_RN24) && !defined(AW_RB24)
#       define AW_RB24(p) AW_RN24(p)
#   elif !defined(AW_RN24) &&  defined(AW_RB24)
#       define AW_RN24(p) AW_RB24(p)
#   endif

#   if    defined(AW_WN24) && !defined(AW_WB24)
#       define AW_WB24(p, v) AW_WN24(p, v)
#   elif !defined(AW_WN24) &&  defined(AW_WB24)
#       define AW_WN24(p, v) AW_WB24(p, v)
#   endif

#   if    defined(AW_RN32) && !defined(AW_RB32)
#       define AW_RB32(p) AW_RN32(p)
#   elif !defined(AW_RN32) &&  defined(AW_RB32)
#       define AW_RN32(p) AW_RB32(p)
#   endif

#   if    defined(AW_WN32) && !defined(AW_WB32)
#       define AW_WB32(p, v) AW_WN32(p, v)
#   elif !defined(AW_WN32) &&  defined(AW_WB32)
#       define AW_WN32(p, v) AW_WB32(p, v)
#   endif

#   if    defined(AW_RN48) && !defined(AW_RB48)
#       define AW_RB48(p) AW_RN48(p)
#   elif !defined(AW_RN48) &&  defined(AW_RB48)
#       define AW_RN48(p) AW_RB48(p)
#   endif

#   if    defined(AW_WN48) && !defined(AW_WB48)
#       define AW_WB48(p, v) AW_WN48(p, v)
#   elif !defined(AW_WN48) &&  defined(AW_WB48)
#       define AW_WN48(p, v) AW_WB48(p, v)
#   endif

#   if    defined(AW_RN64) && !defined(AW_RB64)
#       define AW_RB64(p) AW_RN64(p)
#   elif !defined(AW_RN64) &&  defined(AW_RB64)
#       define AW_RN64(p) AW_RB64(p)
#   endif

#   if    defined(AW_WN64) && !defined(AW_WB64)
#       define AW_WB64(p, v) AW_WN64(p, v)
#   elif !defined(AW_WN64) &&  defined(AW_WB64)
#       define AW_WN64(p, v) AW_WB64(p, v)
#   endif

#else /* AW_HAVE_BIGENDIAN */

#   if    defined(AW_RN16) && !defined(AW_RL16)
#       define AW_RL16(p) AW_RN16(p)
#   elif !defined(AW_RN16) &&  defined(AW_RL16)
#       define AW_RN16(p) AW_RL16(p)
#   endif

#   if    defined(AW_WN16) && !defined(AW_WL16)
#       define AW_WL16(p, v) AW_WN16(p, v)
#   elif !defined(AW_WN16) &&  defined(AW_WL16)
#       define AW_WN16(p, v) AW_WL16(p, v)
#   endif

#   if    defined(AW_RN24) && !defined(AW_RL24)
#       define AW_RL24(p) AW_RN24(p)
#   elif !defined(AW_RN24) &&  defined(AW_RL24)
#       define AW_RN24(p) AW_RL24(p)
#   endif

#   if    defined(AW_WN24) && !defined(AW_WL24)
#       define AW_WL24(p, v) AW_WN24(p, v)
#   elif !defined(AW_WN24) &&  defined(AW_WL24)
#       define AW_WN24(p, v) AW_WL24(p, v)
#   endif

#   if    defined(AW_RN32) && !defined(AW_RL32)
#       define AW_RL32(p) AW_RN32(p)
#   elif !defined(AW_RN32) &&  defined(AW_RL32)
#       define AW_RN32(p) AW_RL32(p)
#   endif

#   if    defined(AW_WN32) && !defined(AW_WL32)
#       define AW_WL32(p, v) AW_WN32(p, v)
#   elif !defined(AW_WN32) &&  defined(AW_WL32)
#       define AW_WN32(p, v) AW_WL32(p, v)
#   endif

#   if    defined(AW_RN48) && !defined(AW_RL48)
#       define AW_RL48(p) AW_RN48(p)
#   elif !defined(AW_RN48) &&  defined(AW_RL48)
#       define AW_RN48(p) AW_RL48(p)
#   endif

#   if    defined(AW_WN48) && !defined(AW_WL48)
#       define AW_WL48(p, v) AW_WN48(p, v)
#   elif !defined(AW_WN48) &&  defined(AW_WL48)
#       define AW_WN48(p, v) AW_WL48(p, v)
#   endif

#   if    defined(AW_RN64) && !defined(AW_RL64)
#       define AW_RL64(p) AW_RN64(p)
#   elif !defined(AW_RN64) &&  defined(AW_RL64)
#       define AW_RN64(p) AW_RL64(p)
#   endif

#   if    defined(AW_WN64) && !defined(AW_WL64)
#       define AW_WL64(p, v) AW_WN64(p, v)
#   elif !defined(AW_WN64) &&  defined(AW_WL64)
#       define AW_WN64(p, v) AW_WL64(p, v)
#   endif

#endif /* !AW_HAVE_BIGENDIAN */

/*
 * Define AW_[RW]N helper macros to simplify definitions not provided
 * by per-arch headers.
 */

#if defined(__GNUC__) && !defined(__TI_COMPILER_VERSION__)

union unaligned_64 { uint64_t l; } __attribute__((packed)) aw_alias;
union unaligned_32 { uint32_t l; } __attribute__((packed)) aw_alias;
union unaligned_16 { uint16_t l; } __attribute__((packed)) aw_alias;

#   define AW_RN(s, p) (((const union unaligned_##s *) (p))->l)
#   define AW_WN(s, p, v) ((((union unaligned_##s *) (p))->l) = (v))

#elif defined(__DECC)

#   define AW_RN(s, p) (*((const __unaligned uint##s##_t*)(p)))
#   define AW_WN(s, p, v) (*((__unaligned uint##s##_t*)(p)) = (v))

#elif AW_HAVE_FAST_UNALIGNED

#   define AW_RN(s, p) (((const aw_alias##s*)(p))->u##s)
#   define AW_WN(s, p, v) (((aw_alias##s*)(p))->u##s = (v))

#else

#ifndef AW_RB16
#   define AW_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])
#endif
#ifndef AW_WB16
#   define AW_WB16(p, darg) do {                \
        unsigned d = (darg);                    \
        ((uint8_t*)(p))[1] = (d);               \
        ((uint8_t*)(p))[0] = (d)>>8;            \
    } while(0)
#endif

#ifndef AW_RL16
#   define AW_RL16(x)                           \
    ((((const uint8_t*)(x))[1] << 8) |          \
      ((const uint8_t*)(x))[0])
#endif
#ifndef AW_WL16
#   define AW_WL16(p, darg) do {                \
        unsigned d = (darg);                    \
        ((uint8_t*)(p))[0] = (d);               \
        ((uint8_t*)(p))[1] = (d)>>8;            \
    } while(0)
#endif

#ifndef AW_RB32
#   define AW_RB32(x)                                \
    (((uint32_t)((const uint8_t*)(x))[0] << 24) |    \
               (((const uint8_t*)(x))[1] << 16) |    \
               (((const uint8_t*)(x))[2] <<  8) |    \
                ((const uint8_t*)(x))[3])
#endif
#ifndef AW_WB32
#   define AW_WB32(p, darg) do {                \
        unsigned d = (darg);                    \
        ((uint8_t*)(p))[3] = (d);               \
        ((uint8_t*)(p))[2] = (d)>>8;            \
        ((uint8_t*)(p))[1] = (d)>>16;           \
        ((uint8_t*)(p))[0] = (d)>>24;           \
    } while(0)
#endif

#ifndef AW_RL32
#   define AW_RL32(x)                                \
    (((uint32_t)((const uint8_t*)(x))[3] << 24) |    \
               (((const uint8_t*)(x))[2] << 16) |    \
               (((const uint8_t*)(x))[1] <<  8) |    \
                ((const uint8_t*)(x))[0])
#endif
#ifndef AW_WL32
#   define AW_WL32(p, darg) do {                \
        unsigned d = (darg);                    \
        ((uint8_t*)(p))[0] = (d);               \
        ((uint8_t*)(p))[1] = (d)>>8;            \
        ((uint8_t*)(p))[2] = (d)>>16;           \
        ((uint8_t*)(p))[3] = (d)>>24;           \
    } while(0)
#endif

#ifndef AW_RB64
#   define AW_RB64(x)                                   \
    (((uint64_t)((const uint8_t*)(x))[0] << 56) |       \
     ((uint64_t)((const uint8_t*)(x))[1] << 48) |       \
     ((uint64_t)((const uint8_t*)(x))[2] << 40) |       \
     ((uint64_t)((const uint8_t*)(x))[3] << 32) |       \
     ((uint64_t)((const uint8_t*)(x))[4] << 24) |       \
     ((uint64_t)((const uint8_t*)(x))[5] << 16) |       \
     ((uint64_t)((const uint8_t*)(x))[6] <<  8) |       \
      (uint64_t)((const uint8_t*)(x))[7])
#endif
#ifndef AW_WB64
#   define AW_WB64(p, darg) do {                \
        uint64_t d = (darg);                    \
        ((uint8_t*)(p))[7] = (d);               \
        ((uint8_t*)(p))[6] = (d)>>8;            \
        ((uint8_t*)(p))[5] = (d)>>16;           \
        ((uint8_t*)(p))[4] = (d)>>24;           \
        ((uint8_t*)(p))[3] = (d)>>32;           \
        ((uint8_t*)(p))[2] = (d)>>40;           \
        ((uint8_t*)(p))[1] = (d)>>48;           \
        ((uint8_t*)(p))[0] = (d)>>56;           \
    } while(0)
#endif

#ifndef AW_RL64
#   define AW_RL64(x)                                   \
    (((uint64_t)((const uint8_t*)(x))[7] << 56) |       \
     ((uint64_t)((const uint8_t*)(x))[6] << 48) |       \
     ((uint64_t)((const uint8_t*)(x))[5] << 40) |       \
     ((uint64_t)((const uint8_t*)(x))[4] << 32) |       \
     ((uint64_t)((const uint8_t*)(x))[3] << 24) |       \
     ((uint64_t)((const uint8_t*)(x))[2] << 16) |       \
     ((uint64_t)((const uint8_t*)(x))[1] <<  8) |       \
      (uint64_t)((const uint8_t*)(x))[0])
#endif
#ifndef AW_WL64
#   define AW_WL64(p, darg) do {                \
        uint64_t d = (darg);                    \
        ((uint8_t*)(p))[0] = (d);               \
        ((uint8_t*)(p))[1] = (d)>>8;            \
        ((uint8_t*)(p))[2] = (d)>>16;           \
        ((uint8_t*)(p))[3] = (d)>>24;           \
        ((uint8_t*)(p))[4] = (d)>>32;           \
        ((uint8_t*)(p))[5] = (d)>>40;           \
        ((uint8_t*)(p))[6] = (d)>>48;           \
        ((uint8_t*)(p))[7] = (d)>>56;           \
    } while(0)
#endif

#if AW_HAVE_BIGENDIAN
#   define AW_RN(s, p)    AW_RB##s(p)
#   define AW_WN(s, p, v) AW_WB##s(p, v)
#else
#   define AW_RN(s, p)    AW_RL##s(p)
#   define AW_WN(s, p, v) AW_WL##s(p, v)
#endif

#endif /* HAVE_FAST_UNALIGNED */

#ifndef AW_RN16
#   define AW_RN16(p) AW_RN(16, p)
#endif

#ifndef AW_RN32
#   define AW_RN32(p) AW_RN(32, p)
#endif

#ifndef AW_RN64
#   define AW_RN64(p) AW_RN(64, p)
#endif

#ifndef AW_WN16
#   define AW_WN16(p, v) AW_WN(16, p, v)
#endif

#ifndef AW_WN32
#   define AW_WN32(p, v) AW_WN(32, p, v)
#endif

#ifndef AW_WN64
#   define AW_WN64(p, v) AW_WN(64, p, v)
#endif

#if AW_HAVE_BIGENDIAN
#   define AW_RB(s, p)    AW_RN##s(p)
#   define AW_WB(s, p, v) AW_WN##s(p, v)
#   define AW_RL(s, p)    aw_bswap##s(AW_RN##s(p))
#   define AW_WL(s, p, v) AW_WN##s(p, aw_bswap##s(v))
#else
#   define AW_RB(s, p)    aw_bswap##s(AW_RN##s(p))
#   define AW_WB(s, p, v) AW_WN##s(p, aw_bswap##s(v))
#   define AW_RL(s, p)    AW_RN##s(p)
#   define AW_WL(s, p, v) AW_WN##s(p, v)
#endif

#define AW_RB8(x)     (((const uint8_t*)(x))[0])
#define AW_WB8(p, d)  do { ((uint8_t*)(p))[0] = (d); } while(0)

#define AW_RL8(x)     AW_RB8(x)
#define AW_WL8(p, d)  AW_WB8(p, d)

#ifndef AW_RB16
#   define AW_RB16(p)    AW_RB(16, p)
#endif
#ifndef AW_WB16
#   define AW_WB16(p, v) AW_WB(16, p, v)
#endif

#ifndef AW_RL16
#   define AW_RL16(p)    AW_RL(16, p)
#endif
#ifndef AW_WL16
#   define AW_WL16(p, v) AW_WL(16, p, v)
#endif

#ifndef AW_RB32
#   define AW_RB32(p)    AW_RB(32, p)
#endif
#ifndef AW_WB32
#   define AW_WB32(p, v) AW_WB(32, p, v)
#endif

#ifndef AW_RL32
#   define AW_RL32(p)    AW_RL(32, p)
#endif
#ifndef AW_WL32
#   define AW_WL32(p, v) AW_WL(32, p, v)
#endif

#ifndef AW_RB64
#   define AW_RB64(p)    AW_RB(64, p)
#endif
#ifndef AW_WB64
#   define AW_WB64(p, v) AW_WB(64, p, v)
#endif

#ifndef AW_RL64
#   define AW_RL64(p)    AW_RL(64, p)
#endif
#ifndef AW_WL64
#   define AW_WL64(p, v) AW_WL(64, p, v)
#endif

#ifndef AW_RB24
#   define AW_RB24(x)                           \
    ((((const uint8_t*)(x))[0] << 16) |         \
     (((const uint8_t*)(x))[1] <<  8) |         \
      ((const uint8_t*)(x))[2])
#endif
#ifndef AW_WB24
#   define AW_WB24(p, d) do {                   \
        ((uint8_t*)(p))[2] = (d);               \
        ((uint8_t*)(p))[1] = (d)>>8;            \
        ((uint8_t*)(p))[0] = (d)>>16;           \
    } while(0)
#endif

#ifndef AW_RL24
#   define AW_RL24(x)                           \
    ((((const uint8_t*)(x))[2] << 16) |         \
     (((const uint8_t*)(x))[1] <<  8) |         \
      ((const uint8_t*)(x))[0])
#endif
#ifndef AW_WL24
#   define AW_WL24(p, d) do {                   \
        ((uint8_t*)(p))[0] = (d);               \
        ((uint8_t*)(p))[1] = (d)>>8;            \
        ((uint8_t*)(p))[2] = (d)>>16;           \
    } while(0)
#endif

#ifndef AW_RB48
#   define AW_RB48(x)                                     \
    (((uint64_t)((const uint8_t*)(x))[0] << 40) |         \
     ((uint64_t)((const uint8_t*)(x))[1] << 32) |         \
     ((uint64_t)((const uint8_t*)(x))[2] << 24) |         \
     ((uint64_t)((const uint8_t*)(x))[3] << 16) |         \
     ((uint64_t)((const uint8_t*)(x))[4] <<  8) |         \
      (uint64_t)((const uint8_t*)(x))[5])
#endif
#ifndef AW_WB48
#   define AW_WB48(p, darg) do {                \
        uint64_t d = (darg);                    \
        ((uint8_t*)(p))[5] = (d);               \
        ((uint8_t*)(p))[4] = (d)>>8;            \
        ((uint8_t*)(p))[3] = (d)>>16;           \
        ((uint8_t*)(p))[2] = (d)>>24;           \
        ((uint8_t*)(p))[1] = (d)>>32;           \
        ((uint8_t*)(p))[0] = (d)>>40;           \
    } while(0)
#endif

#ifndef AW_RL48
#   define AW_RL48(x)                                     \
    (((uint64_t)((const uint8_t*)(x))[5] << 40) |         \
     ((uint64_t)((const uint8_t*)(x))[4] << 32) |         \
     ((uint64_t)((const uint8_t*)(x))[3] << 24) |         \
     ((uint64_t)((const uint8_t*)(x))[2] << 16) |         \
     ((uint64_t)((const uint8_t*)(x))[1] <<  8) |         \
      (uint64_t)((const uint8_t*)(x))[0])
#endif
#ifndef AW_WL48
#   define AW_WL48(p, darg) do {                \
        uint64_t d = (darg);                    \
        ((uint8_t*)(p))[0] = (d);               \
        ((uint8_t*)(p))[1] = (d)>>8;            \
        ((uint8_t*)(p))[2] = (d)>>16;           \
        ((uint8_t*)(p))[3] = (d)>>24;           \
        ((uint8_t*)(p))[4] = (d)>>32;           \
        ((uint8_t*)(p))[5] = (d)>>40;           \
    } while(0)
#endif

/*
 * The AW_[RW]NA macros access naturally aligned data
 * in a type-safe way.
 */

#define AW_RNA(s, p)    (((const aw_alias##s*)(p))->u##s)
#define AW_WNA(s, p, v) (((aw_alias##s*)(p))->u##s = (v))

#ifndef AW_RN16A
#   define AW_RN16A(p) AW_RNA(16, p)
#endif

#ifndef AW_RN32A
#   define AW_RN32A(p) AW_RNA(32, p)
#endif

#ifndef AW_RN64A
#   define AW_RN64A(p) AW_RNA(64, p)
#endif

#ifndef AW_WN16A
#   define AW_WN16A(p, v) AW_WNA(16, p, v)
#endif

#ifndef AW_WN32A
#   define AW_WN32A(p, v) AW_WNA(32, p, v)
#endif

#ifndef AW_WN64A
#   define AW_WN64A(p, v) AW_WNA(64, p, v)
#endif

/*
 * The AW_COPYxxU macros are suitable for copying data to/from unaligned
 * memory locations.
 */

#define AW_COPYU(n, d, s) AW_WN##n(d, AW_RN##n(s));

#ifndef AW_COPY16U
#   define AW_COPY16U(d, s) AW_COPYU(16, d, s)
#endif

#ifndef AW_COPY32U
#   define AW_COPY32U(d, s) AW_COPYU(32, d, s)
#endif

#ifndef AW_COPY64U
#   define AW_COPY64U(d, s) AW_COPYU(64, d, s)
#endif

#ifndef AW_COPY128U
#   define AW_COPY128U(d, s)                                    \
    do {                                                        \
        AW_COPY64U(d, s);                                       \
        AW_COPY64U((char *)(d) + 8, (const char *)(s) + 8);     \
    } while(0)
#endif

/* Parameters for AW_COPY*, AW_SWAP*, AW_ZERO* must be
 * naturally aligned. They may be implemented using MMX,
 * so emms_c() must be called before using any float code
 * afterwards.
 */

#define AW_COPY(n, d, s) \
    (((aw_alias##n*)(d))->u##n = ((const aw_alias##n*)(s))->u##n)

#ifndef AW_COPY16
#   define AW_COPY16(d, s) AW_COPY(16, d, s)
#endif

#ifndef AW_COPY32
#   define AW_COPY32(d, s) AW_COPY(32, d, s)
#endif

#ifndef AW_COPY64
#   define AW_COPY64(d, s) AW_COPY(64, d, s)
#endif

#ifndef AW_COPY128
#   define AW_COPY128(d, s)                    \
    do {                                       \
        AW_COPY64(d, s);                       \
        AW_COPY64((char*)(d)+8, (char*)(s)+8); \
    } while(0)
#endif

#define AW_SWAP(n, a, b) AWSWAP(aw_alias##n, *(aw_alias##n*)(a), *(aw_alias##n*)(b))

#ifndef AW_SWAP64
#   define AW_SWAP64(a, b) AW_SWAP(64, a, b)
#endif

#define AW_ZERO(n, d) (((aw_alias##n*)(d))->u##n = 0)

#ifndef AW_ZERO16
#   define AW_ZERO16(d) AW_ZERO(16, d)
#endif

#ifndef AW_ZERO32
#   define AW_ZERO32(d) AW_ZERO(32, d)
#endif

#ifndef AW_ZERO64
#   define AW_ZERO64(d) AW_ZERO(64, d)
#endif

#ifndef AW_ZERO128
#   define AW_ZERO128(d)         \
    do {                         \
        AW_ZERO64(d);            \
        AW_ZERO64((char*)(d)+8); \
    } while(0)
#endif

#endif /* AWUTIL_INTREADWRITE_H */
