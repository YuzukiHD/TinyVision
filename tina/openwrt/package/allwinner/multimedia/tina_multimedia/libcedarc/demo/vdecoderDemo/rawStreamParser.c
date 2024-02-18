/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : rawStreamParser.h
 * Description : rawStreamParser
 * History :
 *
 */

#include <stdlib.h>
#include <string.h>


#include "rawStreamParser.h"
#include "cdc_log.h"


#define RAW_STEAM_DATA_LEN (512*1024)


RawParserContext* RawParserOpen(FILE* fd)
{
    RawParserContext* pCtx = NULL;

    pCtx = (RawParserContext*)calloc(1, sizeof(RawParserContext));
    if(pCtx == NULL)
    {
        loge("malloc for rawParserContext failed");
        return NULL;
    }

    pCtx->fd = fd;

    pCtx->mBufManager.pStreamBuffer = (char*)calloc(1, RAW_STEAM_DATA_LEN);
    if(pCtx->mBufManager.pStreamBuffer == NULL)
    {
        loge(" malloc for stream data failed");
        free(pCtx);
        return NULL;
    }
    pCtx->mBufManager.nBufferLen = RAW_STEAM_DATA_LEN;

    return pCtx;
}

int RawParserClose(RawParserContext* pCtx)
{
    if(pCtx)
    {
        if(pCtx->mBufManager.pStreamBuffer)
            free(pCtx->mBufManager.pStreamBuffer);

        free(pCtx);
    }
    return 0;
}

static int probeH264(char *p)
{
    int code, type;
    code = p[0] & 0x80;
    if(code != 0)
    {
        logd(" Maybe this is not h264 stream. forbidden_zero_bit != 0  ");
        return 0;
    }
    type = p[0] & 0x1f;
    if(type <= 13 || type == 19)
    {
        logd(" Maybe this is h264 stream. nal unit type = %d", type);
        return 50;
    }
    else
    {
        logd(" This is not h264 stream. nal unit type = %d", type);
        return 0;
    }
    return 0;
}

static int probeH265(char *p)
{
    int code=0, type=0, nNuhLayerId=0;
    int ret = 0;

    code = p[0] & 0x80;
    if(code != 0)
    {
        logd(" Maybe this is not h265 stream. forbidden_zero_bit != 0  ");
        return ret;
    }
    type = (p[0] & 0x7e) >> 1;
    if(type > 63)
    {
        logd(" Maybe this is not h265 stream. nal unit type > 63");
        return ret;
    }
    switch(type)
    {
        case 0x20: /*VPS*/
        case 0x21: /*SPS*/
        case 0x22: /*PPS*/
        {
            nNuhLayerId = ((p[0] & 0x1) << 5) | ((p[1] & 0xf8) >> 3);
            logd(" Parser probe result: This is h265 raw stream:%d! ",
                nNuhLayerId);
            ret = 50;
            break;
        }
        default:
            break;
    }
    return ret;
}


int RawParserProbe(RawParserContext* pCtx)
{
    char *data = NULL;
    char *p = NULL;
    int mask = 0xffffff;
    int code = -1;
    int i=0, ret=0;
    int nProbeDataLen = 10 * 1024;

    data = calloc(1, nProbeDataLen);
    if(data == NULL)
    {
        loge("malloc for probe data failed");
        return -1;
    }

    fread(data, 1, nProbeDataLen, pCtx->fd);

    for(i = 0; i < nProbeDataLen; i++)
    {
        code = (code << 8) | data[i];
        if((code & mask) == 0x000001) /* h265, h264 start code */
        {
            p = &data[i + 1];
            break;
        }
    }
    if(p != NULL)
    {
        ret = probeH265(p);
        if(ret == 0)
        {
            ret = probeH264(p);
            if(ret != 0)
                pCtx->streamType = VIDEO_FORMAT_H264;

        }
        else
            pCtx->streamType = VIDEO_FORMAT_H265;

    }
    logd("RawParserProbe, pCtx->streamType = %d",pCtx->streamType);
    fseek(pCtx->fd, 0, SEEK_SET);
    free(data);

    return pCtx->streamType;
}

static int supplyData(RawParserContext* pCtx)
{
    int nRealLen = 0;
    if(pCtx->mBufManager.nValidSize <= 0)
    {
        nRealLen = fread(pCtx->mBufManager.pStreamBuffer, 1, \
            pCtx->mBufManager.nBufferLen, pCtx->fd);
        if(nRealLen <= 0)
            return -1;
        else
        {
            pCtx->mBufManager.nValidSize = nRealLen;
            pCtx->mBufManager.nCurIndex = 0;
        }
    }
    else
    {
        char* tmpBuf = calloc(1, pCtx->mBufManager.nValidSize);
        if(tmpBuf == NULL)
        {
            loge("malloc for tmpBuf, size = %d",pCtx->mBufManager.nValidSize);
            return -1;
        }
        memcpy(tmpBuf, (pCtx->mBufManager.pStreamBuffer + pCtx->mBufManager.nCurIndex),
               pCtx->mBufManager.nValidSize);

        int nLen = pCtx->mBufManager.nBufferLen - pCtx->mBufManager.nValidSize;
        nRealLen = fread(pCtx->mBufManager.pStreamBuffer + pCtx->mBufManager.nValidSize, 1,
                         nLen, pCtx->fd);

        memcpy(pCtx->mBufManager.pStreamBuffer, tmpBuf, pCtx->mBufManager.nValidSize);

        free(tmpBuf);
        tmpBuf = NULL;
        if(nRealLen <= 0)
        {
            return -1;
        }
        else
        {
            pCtx->mBufManager.nValidSize += nRealLen;
            pCtx->mBufManager.nCurIndex = 0;
        }
    }
    return 0;
}

int RawParserPrefetch(RawParserContext* pCtx, DataPacketT* pPkt)
{
    int i = 0;
    char tmpBuf[3];
    int nFindStartCode = 0;
    int nStart = 0;
    int nStreamDataLen = -1;
    int nRet = 0;

    char* pCurDataPtr = NULL;

    if(pCtx->mBufManager.nValidSize <= 0)
    {
        nRet = supplyData(pCtx);
        if(nRet == -1)
            return -1;
    }

find_startCode:

    pCurDataPtr = pCtx->mBufManager.pStreamBuffer + pCtx->mBufManager.nCurIndex;

    logv("data: %x, %x, %x, %x, %x, %x, %x, %x", *(pCurDataPtr), *(pCurDataPtr + 1)
        ,*(pCurDataPtr+2),*(pCurDataPtr+3)
        ,*(pCurDataPtr+4),*(pCurDataPtr+5),*(pCurDataPtr+6),*(pCurDataPtr+7));
    //* find the first startcode
    for(i = 0; i < (pCtx->mBufManager.nValidSize - 3); i++)
    {
        tmpBuf[0] = *(pCurDataPtr + i);
        tmpBuf[1] = *(pCurDataPtr + i + 1);
        tmpBuf[2] = *(pCurDataPtr + i + 2);
        if(tmpBuf[0] == 0 && tmpBuf[1] == 0 && tmpBuf[2] == 1)
        {
            nFindStartCode = 1;
            break;
        }
    }

    logv("nFindStartCode = %d, i = %d, valisSize = %d",\
        nFindStartCode, i, pCtx->mBufManager.nValidSize);
    if(nFindStartCode == 1)
    {
        pCtx->mBufManager.nCurIndex += i;
        nStart = i;
        if(*(pCurDataPtr + i -1) == 0)
        {
            pCtx->mBufManager.nCurIndex -= 1;
            nStart -= 1;
        }
        nFindStartCode = 0;

        //* find the second startcode
        for(i += 3; i < (pCtx->mBufManager.nValidSize - 3); i++)
        {
            logv("pCurDataPtr = %p, i = %d", pCurDataPtr, i);
            tmpBuf[0] = *(pCurDataPtr + i);
            tmpBuf[1] = *(pCurDataPtr + i + 1);
            tmpBuf[2] = *(pCurDataPtr + i + 2);
            if(tmpBuf[0] == 0 && tmpBuf[1] == 0 && tmpBuf[2] == 1)
            {
                nFindStartCode = 1;
                break;
            }
        }

        if(nFindStartCode == 1)
        {

            if(*(pCurDataPtr + i - 1) == 0)
            {
                nStreamDataLen = i - nStart - 1;
            }
            else
            {
                nStreamDataLen = i - nStart;
            }
        }
        else
        {
            supplyData(pCtx);
            if(nRet == -1)
                return -1;

            goto find_startCode;
        }
    }
    else
    {
        supplyData(pCtx);
        if(nRet == -1)
            return -1;

        goto find_startCode;
    }

    pPkt->length = nStreamDataLen;
    logv("pPkt->length = %lld",pPkt->length);
    return 0;
}

int RawParserRead(RawParserContext* pCtx, DataPacketT* pPkt)
{
    if(pPkt->length <= 0)
        return -1;

    char* pCpyPtr = pCtx->mBufManager.pStreamBuffer + pCtx->mBufManager.nCurIndex;

    logv("read data: %x, %x, %x, %x", *pCpyPtr,*(pCpyPtr+1), *(pCpyPtr+2), *(pCpyPtr+3));
    if(pPkt->length <= pPkt->buflen)
    {
        memcpy(pPkt->buf, pCpyPtr, pPkt->length);
    }
    else
    {
        memcpy(pPkt->buf, pCpyPtr, pPkt->buflen);
        memcpy(pPkt->ringBuf, pCpyPtr + pPkt->buflen, pPkt->length - pPkt->buflen);
    }
    pCtx->mBufManager.nCurIndex += pPkt->length;
    pCtx->mBufManager.nValidSize -= pPkt->length;

    return 0;

}


