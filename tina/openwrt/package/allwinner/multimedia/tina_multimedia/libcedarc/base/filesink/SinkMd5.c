/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : copy from h265_md5.c
* Description :
* History :
*   Author  : jilinglin
*   Date    : 2020/10/31
*   Comment :
*/
#include "SinkMd5.h"
#include "Md5Common.h"

#define sink_le2ne64(x)   (x)
static const u8 S[4][4] = {
    { 7, 12, 17, 22 },  /* round 1 */
    { 5,  9, 14, 20 },  /* round 2 */
    { 4, 11, 16, 23 },  /* round 3 */
    { 6, 10, 15, 21 }   /* round 4 */
};

static const u32 T[64] = { // T[i]= fabs(sin(i+1)<<32)
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,   /* round 1 */
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,

    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,   /* round 2 */
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,

    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,   /* round 3 */
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,

    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,   /* round 4 */
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

#define CORE(i, a, b, c, d) do {                                        \
        t = S[i >> 4][i & 3];                                           \
        a += T[i];                                                      \
                                                                        \
        if (i < 32) {                                                   \
            if (i < 16) a += (d ^ (b & (c ^ d))) + X[       i  & 15];   \
            else        a += (c ^ (d & (c ^ b))) + X[(1 + 5*i) & 15];   \
        } else {                                                        \
            if (i < 48) a += (b ^ c ^ d)         + X[(5 + 3*i) & 15];   \
            else        a += (c ^ (b | ~d))      + X[(    7*i) & 15];   \
        }                                                               \
        a = b + (a << t | a >> (32 - t));                               \
    } while (0)

static void body(u32 ABCD[4], u32 X[16])
{
    int t;
    unsigned int a = ABCD[3];
    unsigned int b = ABCD[2];
    unsigned int c = ABCD[1];
    unsigned int d = ABCD[0];

#if HAVE_BIGENDIAN
    int i ;
    for (i = 0; i < 16; i++)
        X[i] = sink_bswap32(X[i]);
#endif

#define CORE2(i)                                                        \
    CORE( i,   a,b,c,d); CORE((i+1),d,a,b,c);                           \
    CORE((i+2),c,d,a,b); CORE((i+3),b,c,d,a)
#define CORE4(i) CORE2(i); CORE2((i+4)); CORE2((i+8)); CORE2((i+12))
    CORE4(0); CORE4(16); CORE4(32); CORE4(48);

    ABCD[0] += d;
    ABCD[1] += c;
    ABCD[2] += b;
    ABCD[3] += a;
}

void SinkMd5Init(SinkMD5 *ctx)
{
    ctx->len     = 0;

    ctx->ABCD[0] = 0x10325476;
    ctx->ABCD[1] = 0x98badcfe;
    ctx->ABCD[2] = 0xefcdab89;
    ctx->ABCD[3] = 0x67452301;
}

void SinkMd5Update(SinkMD5 *ctx,  u8 *src, const int len)
{
    int i, j;

    j = ctx->len & 63;
    ctx->len += len;

    for (i = 0; i < len; i++) {
        ctx->block[j++] = src[i];
        if (j == 64) {
            body(ctx->ABCD, (u32 *) ctx->block);
            j = 0;
        }
    }
}

void SinkMd5Final(SinkMD5 *ctx, u8 *dst)
{
    int i;
    u64 finalcount = sink_le2ne64(ctx->len << 3);

    SinkMd5Update(ctx, (u8 *)"\200", 1);
    while ((ctx->len & 63) != 56)
        SinkMd5Update(ctx, (u8 *)"", 1);

    SinkMd5Update(ctx, (u8 *)&finalcount, 8);

    for (i = 0; i < 4; i++)
        SINK_WL32(dst + 4*i, ctx->ABCD[3 - i]);
}
