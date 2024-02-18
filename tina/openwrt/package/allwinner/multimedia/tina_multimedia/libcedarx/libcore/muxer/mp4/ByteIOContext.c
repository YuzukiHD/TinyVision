
#define LOG_TAG "ByteIOContext"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include "ByteIOContext.h"

void put_buffer_cache(void *mov, ByteIOContext *s, char *buf, int size)
{
    s->fsWrite(s, buf, size);
    return;
}

void put_byte_cache(void *mov, ByteIOContext *s, int b)
{
    put_buffer_cache(mov, s, (char*)&b, 1);
}

void put_le32_cache(void *mov, ByteIOContext *s, unsigned int val)
{
	put_buffer_cache(mov, s, (char*)&val, 4);
}

void put_be32_cache(void *mov, ByteIOContext *s, unsigned int val)
{
	val= ((val<<8)&0xFF00FF00) | ((val>>8)&0x00FF00FF);
    val= (val>>16) | (val<<16);
    put_buffer_cache(mov, s, (char*)&val, 4);
}

void put_be16_cache(void *mov, ByteIOContext *s, unsigned int val)
{
    put_byte_cache(mov, s, val >> 8);
    put_byte_cache(mov, s, val);
}

void put_be24_cache(void *mov, ByteIOContext *s, unsigned int val)
{
    put_be16_cache(mov, s, val >> 8);
    put_byte_cache(mov, s, val);
}

void put_tag_cache(void *mov, ByteIOContext *s, const char *tag)
{
    while (*tag) {
        put_byte_cache(mov, s, *tag++);
    }
}

