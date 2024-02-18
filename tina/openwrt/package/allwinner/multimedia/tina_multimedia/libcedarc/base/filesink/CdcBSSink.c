/*
 * =====================================================================================
 *
 *       Filename:  CdcFileSink.c
 *
 *    Description:  for save bitstream
 *
 *        Version:  1.0
 *        Created:  2020年10月22日 14时31分53秒
 *       Revision:  none
 *       Compiler:  gcc
 *      CodeStyle:  android
 *
 *         Author:  jilinglin
 *        Company:  allwinnertech.com
 *
 * =====================================================================================
 */

#include "CdcBSSink.h"
#include "cdc_log.h"
#include "typedef.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


typedef struct CdcBSSink {
    CdcSinkInterface mInterFace;
    FILE* pFile;
    char mFileName[30];
} CdcBSSink;

int BSSinkWriteBS(CdcSinkInterface* pSelf, CdcBitStream* pStream)
{
    CdcBSSink* pContext = (CdcBSSink*)pSelf;
    FILE* pFile = NULL;

    if(pContext->pFile == NULL)
    {
        logw("file not open");
        return -1;
    }
    if(pStream == NULL)
    {
        logw("bitstream is null");
        return -1;
    }

    pFile = pContext->pFile;

    if(pStream->nAddHead)
    {
        fwrite(&(pStream->mHead.mMask), 1, 4, pFile);
        fwrite(&(pStream->mHead.mBSIndex), 1, sizeof(int), pFile);
        fwrite(&(pStream->mHead.mLen), 1, sizeof(int), pFile);
        fwrite(&(pStream->mHead.mPts), 1, sizeof(int), pFile);
    }

    fwrite(pStream->pData, 1, pStream->nLen, pFile);
    logd("save bitstream size:%d", pStream->nLen);
    return 0;
}

int BSSinkWritePic(CdcSinkInterface* pSelf, CdcSinkPicture* pPic)
{
    (void)pSelf;
    (void)pPic;
    logd("bitstream sink not support save pic");
    return -1;
}

int BSSinkWriteSinglePic(CdcSinkInterface* pSelf, CdcSinkPicture* pPic)
{
    (void)pSelf;
    (void)pPic;
    logd("bitstream sink not support save pic");
    return -1;
}

int BSSinkSetParam(CdcSinkInterface* pSelf, CdcSinkParam* pParam)
{
    CdcBSSink* pContext = (CdcBSSink*)pSelf;
    if(pParam == NULL)
    {
        loge("param is null");
        return -1;
    }

    memcpy(pContext->mFileName, pParam->pFileName, pParam->nLen);

    pContext->pFile = fopen(pContext->mFileName, "wb");
    if(pContext->pFile == NULL)
    {
        loge("open file failure, name:%s", pContext->mFileName);
        return -1;
    }
    return 0;
}

CdcSinkInterface* CdcBSSinkCreate(void)
{
    CdcBSSink* pContext = NULL;
    pContext = (CdcBSSink*)malloc(sizeof(CdcBSSink));
    if(pContext == NULL)
    {
        loge("malloc failure");
        return NULL;
    }
    memset(pContext, 0, sizeof(CdcBSSink));

    pContext->mInterFace.pWritePicture = BSSinkWritePic;
    pContext->mInterFace.pWriteSinglePicture = BSSinkWriteSinglePic;
    pContext->mInterFace.pWriteBitStream = BSSinkWriteBS;
    pContext->mInterFace.pSetParameter = BSSinkSetParam;

    return &(pContext->mInterFace);

}


void CdcBSSinkDestory(CdcSinkInterface* pSelf)
{
    CdcBSSink* pContext = (CdcBSSink*)pSelf;
    if(pContext->pFile != NULL)
        fclose(pContext->pFile);
    free(pContext);
    pContext = NULL;
}
