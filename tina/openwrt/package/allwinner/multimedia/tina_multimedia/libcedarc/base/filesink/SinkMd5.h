/*
 * =====================================================================================
 *
 *       Filename:  SinkMd5.h
 *
 *    Description:  for md5 calculate
 *
 *        Version:  1.0
 *        Created:  2020年10月30日 16时04分47秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  jilinglin
 *        Company:  allwinnertech.com
 *
 * =====================================================================================
 */
#ifndef SINK_MD5_FILE
#define SINK_MD5_FILE
#include "typedef.h"

typedef struct SinkMD5
{
    u64 len;
    u8  block[64];
    u32 ABCD[4];
} SinkMD5;

void SinkMd5Final(SinkMD5 *ctx, u8 *dst);
void SinkMd5Update(SinkMD5 *ctx,  u8 *src, const int len);
void SinkMd5Init(SinkMD5 *ctx);

#endif
