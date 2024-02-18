/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : h265_common.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#ifndef H265_COMMON_H
#define H265_COMMON_H

#include "typedef.h"

union unaligned_32 { u32 l; } __attribute__((packed)) ;

#define SINK_RN(s, p) (((const union unaligned_##s *) (p))->l)
#define SINK_WN(s, p, v) ((((union unaligned_##s *) (p))->l) = (v))

#define SINK_RN32(p) SINK_RN(32, p)
#define SINK_WN32(p, v) SINK_WN(32, p, v)

#define SINK_BSWAP16C(x) (((x) << 8 & 0xff00)  | ((x) >> 8 & 0x00ff))
#define SINK_BSWAP32C(x) (SINK_BSWAP16C(x) << 16 | SINK_BSWAP16C((x) >> 16))
#define SINK_BSWAP64C(x) (SINK_BSWAP32C(x) << 32 | SINK_BSWAP32C((x) >> 32))
#define SINK_BSWAPC(s, x) SINK_BSWAP##s##C(x)

#define SINK_RB(s, p)    sink_bswap##s(SINK_RN##s(p))
#define SINK_WB(s, p, v) SINK_WN##s(p, sink_bswap##s(v))
#define SINK_RL(s, p)    SINK_RN##s(p)
#define SINK_WL(s, p, v) SINK_WN##s(p, v)
#define SINK_RB32(p)    SINK_RB(32, p)

#define SINK_RB8(x)     (((const u8*)(x))[0])
#define SINK_WB8(p, d)  do { ((u8*)(p))[0] = (d); } while(0)

#define SINK_RL8(x)     SINK_RB8(x)
#define SINK_WL8(p, d)  SINK_WB8(p, d)
#define SINK_WL32(p, v) SINK_WL(32, p, v)

#define MIN_CACHE_BITS 25
#define OPEN_READER(name, gb)                   \
    unsigned int name##_index = (gb)->index;    \
    unsigned int  name##_cache = 0;    \
    unsigned int  name##_size_plus8 =  \
                (gb)->size_in_bits_plus8; \
    CEDARC_UNUSE(name##_cache);    \
    CEDARC_UNUSE(name##_size_plus8)

#define CLOSE_READER(name, gb) (gb)->index = name##_index

#define UPDATE_CACHE(name, gb) name##_cache = \
        SINK_RB32((gb)->buffer + (name##_index >> 3)) << (name##_index & 7)

#define GET_CACHE(name, gb) ((u32)name##_cache)

#define SINK_SKIP_COUNTER(name, gb, num) \
    name##_index = SINKMIN(name##_size_plus8, name##_index + (num))

#define SINK_NEG_SSR32(a,s) ((( s32)(a))>>(32-(s)))
#define SINK_NEG_USR32(a,s) (((u32)(a))>>(32-(s)))

#define LAST_SKIP_BITS(name, gb, num) SINK_SKIP_COUNTER(name, gb, num)
#define SHOW_UBITS(name, gb, num) SINK_NEG_USR32(name##_cache, num)
#define SHOW_SBITS(name, gb, num) SINK_NEG_SSR32(name##_cache, num)

#define sink_log2       sink_log2_c
#define SINKMIN(a,b) ((a) > (b) ? (b) : (a))

#endif //H265_COMMON_H

