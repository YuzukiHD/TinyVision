/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : rawStreamParser.h
 * Description : rawStreamParser
 * History :
 *
 */

#include <stdio.h>
#include <stdint.h>

#ifndef RAW_STREAM_PARSER_H
#define RAW_STREAM_PARSER_H

#define VIDEO_FORMAT_H264 0x1
#define VIDEO_FORMAT_H265 0x2

typedef struct DataPacketS
{
    void *buf;
    void *ringBuf;
    int buflen;
    int ringBufLen;
    int64_t pts;
    int64_t length;
    int64_t pcr;
}DataPacketT;

typedef struct StreamBufMan{
    char* pStreamBuffer;
    int nCurIndex;
    int nBufferLen;
    int nValidSize;
}StreamBufMan;

typedef struct RawParserContext{
    FILE* fd;
    int streamType;
    StreamBufMan mBufManager;
}RawParserContext;

RawParserContext* RawParserOpen(FILE* fd);
int RawParserClose(RawParserContext* pCtx);
int RawParserProbe(RawParserContext* pCtx);
int RawParserPrefetch(RawParserContext* pCtx, DataPacketT* pPkt);
int RawParserRead(RawParserContext* pCtx, DataPacketT* pPkt);

#endif
