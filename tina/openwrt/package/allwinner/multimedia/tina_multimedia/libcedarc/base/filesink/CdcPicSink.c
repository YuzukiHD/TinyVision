/*
 * =====================================================================================
 *
 *       Filename:  CdcFileSink.c
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2020年10月22日 14时31分53秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  jilinglin
 *        Company:  allwinnertech.com
 *
 * =====================================================================================
 */
#include "CdcPicSink.h"
#include "cdc_log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "SinkMd5.h"

#define PIC_ALIGN(value, n) (((value) + (n) - 1) & ~((n) - 1))

typedef struct CdcPicSink {
    CdcSinkInterface mInterFace;
    char mFileName[30];
    FILE* pFile;
    int mStartNum;
    int mTotalNum;
    int mPicCount;
    int bMd5Open;
    SinkMD5 mMd5Ctx;
} CdcPicSink;

int PicSinkWriteBS(CdcSinkInterface* pSelf, CdcBitStream* pStream)
{
    (void)pSelf;
    (void)pStream;
    loge("not support");
    return -1;
}

static int InterUpdateMd5Data(CdcPicSink* pContext, CdcSinkPicture* pPic, u8 mMd5Ret[16])
{
    int mTopOffset    = pPic->nTop;
    int mLeftOffset   = pPic->nLeft;
    int mBottomOffset = pPic->nBottom;
    int mRightOffset  = pPic->nRight;
    int mCopyHeight   = mBottomOffset - mTopOffset;
    int mCopyWidth    = mRightOffset - mLeftOffset;
    int mStride       = PIC_ALIGN(pPic->nWidth, pPic->nAlign);
    u8* pSrc = (u8*)pPic->pData[0] + (mStride * mTopOffset + mLeftOffset);
    int i = 0;
    SinkMD5* pMd5Ctx = &(pContext->mMd5Ctx);

    SinkMd5Init(pMd5Ctx);
    //data[0], for all
    for(i=0; i<mCopyHeight; i++)
    {
        SinkMd5Update(pMd5Ctx, (void*)pSrc, mCopyWidth);
        pSrc += mStride;
    }

    //data[1]
    if(pPic->nFormat == CDC_SINK_PIC_FORMAT_NV12 || pPic->nFormat == CDC_SINK_PIC_FORMAT_NV21)
    {
        pSrc =((u8*)pPic->pData[1]) + mStride * mTopOffset / 2 + mLeftOffset;
        for(i=0; i<mCopyHeight/2 ; i++)
        {
            SinkMd5Update(pMd5Ctx, pSrc, mCopyWidth);
            pSrc += mStride;
        }
        SinkMd5Final(pMd5Ctx, mMd5Ret);
        return 0;
    }

    if(pPic->nFormat == CDC_SINK_PIC_FORMAT_YU12 || pPic->nFormat == CDC_SINK_PIC_FORMAT_YV12)
    {
        pSrc =((u8*)pPic->pData[1]) + mStride/2 * mTopOffset / 2 + mLeftOffset / 2;
        mStride /= 2;
        mCopyHeight /= 2;
        mCopyWidth /= 2;
    }
    else
    {
        pSrc =((u8*)pPic->pData[1]) + mStride * mTopOffset + mLeftOffset;
    }

    for(i=0; i<mCopyHeight; i++)
    {
        SinkMd5Update(pMd5Ctx, pSrc, mCopyWidth);
        pSrc += mStride;
    }

    //for data[2]
    if(pPic->nFormat == CDC_SINK_PIC_FORMAT_YU12 || pPic->nFormat == CDC_SINK_PIC_FORMAT_YV12)
    {
        pSrc =((u8*)pPic->pData[2]) + mStride/2 * mTopOffset / 2 + mLeftOffset / 2;
        for(i=0; i<mCopyHeight; i++)
        {
            SinkMd5Update(pMd5Ctx, pSrc, mCopyWidth);
            pSrc += mStride;
        }
        SinkMd5Final(pMd5Ctx, mMd5Ret);
        return 0;
    }
    else
    {
        pSrc =((u8*)pPic->pData[2]) + mStride * mTopOffset + mLeftOffset;
        mStride *= 2;
        mCopyHeight *= 2;
        mCopyWidth *= 2;
        for(i=0; i<mCopyHeight; i++)
        {
            SinkMd5Update(pMd5Ctx, pSrc, mCopyWidth);
            pSrc += mStride;
        }
    }

    //for data[3]
    pSrc =((u8*)pPic->pData[3]) + mStride * mTopOffset + mLeftOffset;
    for(i=0; i<mCopyHeight; i++)
    {
        SinkMd5Update(pMd5Ctx, pSrc, mCopyWidth);
        pSrc += mStride;
    }
    SinkMd5Final(pMd5Ctx, mMd5Ret);
    return 0;
}

static int InterPicSaveMd5(CdcPicSink* pContext, CdcSinkPicture* pPic)
{
    SinkMD5* pMd5Ctx = NULL;
    u8 mMd5Ret[16];
    int i;
    pMd5Ctx = &(pContext->mMd5Ctx);

    if(pPic->nAfbc)
    {
        SinkMd5Init(pMd5Ctx);
        SinkMd5Update(pMd5Ctx, pPic->pData[0], pPic->nAfbcSize);
        SinkMd5Final(pMd5Ctx, mMd5Ret);
    }
    else
    {
        if(pPic->nBpp == 10)
        {
            loge("now just not support");
            return -1;
        }
        else
        {
            InterUpdateMd5Data(pContext, pPic, mMd5Ret);
        }
    }

    for(i = 0; i < sizeof(mMd5Ret); i++) {
        fprintf(pContext->pFile, "%02X", mMd5Ret[i]);
    }
    fprintf(pContext->pFile, "\n");
    fflush(pContext->pFile);
    logd("pic:%p data:%x %x %x %x %x %x %x %x",
        (u8*)pPic->pData[0],mMd5Ret[0],mMd5Ret[1],mMd5Ret[2],mMd5Ret[3],mMd5Ret[4],mMd5Ret[5],mMd5Ret[6],mMd5Ret[7]);
    return 0;
}

int PicSinkWritePic(CdcSinkInterface* pSelf, CdcSinkPicture* pPic)
{
    int nLen;

    CdcPicSink* pContext = (CdcPicSink*)pSelf;
    if(pPic == NULL || pContext->pFile == NULL)
    {
        return -1;
    }

    if(pContext->mPicCount < pContext->mStartNum ||
       pContext->mPicCount >= pContext->mStartNum + pContext->mTotalNum)
    {
        return 0;
    }

    if(pContext->bMd5Open)
    {
        InterPicSaveMd5(pContext, pPic);
        goto EXIT;
    }
    if(pPic->nAfbc)
    {
        fwrite(pPic->pData[0], 1, pPic->nAfbcSize, pContext->pFile);
        goto EXIT;
    }

    nLen = PIC_ALIGN(pPic->nWidth, pPic->nAlign)*PIC_ALIGN(pPic->nHeight, pPic->nAlign);
    switch(pPic->nFormat)
    {
        case CDC_SINK_PIC_FORMAT_NV12:
        case CDC_SINK_PIC_FORMAT_NV21:
            if(pPic->nBpp == 10)
                nLen *= 2;
            fwrite(pPic->pData[0], 1, nLen, pContext->pFile);
            fwrite(pPic->pData[1], 1, nLen/2, pContext->pFile);
            break;
        case CDC_SINK_PIC_FORMAT_YU12:
        case CDC_SINK_PIC_FORMAT_YV12:
            if(pPic->nBpp == 10)
                nLen *= 2;

            fwrite(pPic->pData[0], 1, nLen, pContext->pFile);
            fwrite(pPic->pData[1], 1, nLen/4, pContext->pFile);
            fwrite(pPic->pData[2], 1, nLen/4, pContext->pFile);
            break;
        case CDC_SINK_PIC_FORMAT_ARGB:
            fwrite(pPic->pData[0], 1, nLen, pContext->pFile);
            fwrite(pPic->pData[1], 1, nLen, pContext->pFile);
            fwrite(pPic->pData[2], 1, nLen, pContext->pFile);
            fwrite(pPic->pData[3], 1, nLen, pContext->pFile);
            break;
        default:
            loge("picture format not support");
    }

EXIT:
    logd("save pic width:%d height:%d format:%d", pPic->nWidth, pPic->nHeight, pPic->nFormat);
    pContext->mPicCount++;

    return 0;
}

int PicSinkWriteSinglePic(CdcSinkInterface* pSelf, CdcSinkPicture* pPic)
{
    (void)pSelf;
    (void)pPic;
    logd("not support");
    return 0;
}

int PicSinkSetParam(CdcSinkInterface* pSelf, CdcSinkParam* pParam)
{
    CdcPicSink* pContext = (CdcPicSink*)pSelf;
    if(pParam == NULL)
    {
        loge("param is null");
        return -1;
    }

    memcpy(pContext->mFileName, pParam->pFileName, pParam->nLen);
    pContext->mStartNum = pParam->nStartNum;
    pContext->mTotalNum = pParam->nToatalNum;
    pContext->bMd5Open = pParam->bMd5Open;
    pContext->pFile = fopen(pContext->mFileName, "wb");
    if(pContext->pFile == NULL)
    {
        loge("open file failure, name:%s", pContext->mFileName);
        return -1;
    }
    return 0;
}

CdcSinkInterface* CdcPicSinkCreate(void)
{
    CdcPicSink* pContext = NULL;
    pContext = (CdcPicSink*)malloc(sizeof(CdcPicSink));
    if(pContext == NULL)
    {
        loge("malloc failure");
        return NULL;
    }
    memset(pContext, 0, sizeof(CdcPicSink));

    pContext->mInterFace.pWritePicture = PicSinkWritePic;
    pContext->mInterFace.pWriteSinglePicture = PicSinkWriteSinglePic;
    pContext->mInterFace.pWriteBitStream = PicSinkWriteBS;
    pContext->mInterFace.pSetParameter = PicSinkSetParam;

    return &(pContext->mInterFace);

}


void CdcPicSinkDestory(CdcSinkInterface* pSelf)
{
    CdcPicSink* pContext = (CdcPicSink*)pSelf;
    if(pContext->pFile != NULL)
        fclose(pContext->pFile);
    free(pContext);
    pContext = NULL;
}
