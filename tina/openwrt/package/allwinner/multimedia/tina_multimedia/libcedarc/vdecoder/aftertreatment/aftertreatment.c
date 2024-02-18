/*
 * =====================================================================================
 *
 *       Filename:  aftertreatment.c
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2020年07月28日 16时11分45秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *        Company:
 *
 * =====================================================================================
 */
#include "aftertreatment.h"
#include "scaledown.h"
#include "cdc_log.h"
#include "pthread.h"

typedef struct PicNode {
    VideoPicture  vPicture;
    struct PicNode*    pNext;
} PicNode;

typedef struct ATMConfig {
    int nFrameNum;
    int nInterBufFlag;
    int nPixelFormat;
    int nAlignSize;
    int nPicWidth;
    int nPicHeight;
    Fbm* pFbm;
} ATMConfig;

typedef struct ATMContext {
    ATMConfig mConfig;
    PicNode* pFrameArray;
    struct ScMemOpsS* pMemOps;
    VeOpsS* pVeOps;
    void* pVeOpsSelf;
    int nValidBufNum;
    PicNode* pValidQueue;
    PicNode* pEmptyQueue;
    int nValidPicNum;
    int nEmptyPicNum;
    SDInstant pScaleDown;
    pthread_mutex_t nMutex;
} ATMContext;

#define ATM_ALIGN(value, n) (((value) + (n) - 1) & ~((n) - 1))

static PicNode* InterDequeue(PicNode** ppHead)
{
    PicNode* pHead;

    pHead = *ppHead;

    if(pHead == NULL)
        return NULL;
    else
    {
        *ppHead = pHead->pNext;
        pHead->pNext = NULL;
        return pHead;
    }
}

static void InterEnqueue(PicNode** ppHead, PicNode* p)
{
    PicNode* pCur;

    pCur = *ppHead;

    if(pCur == NULL)
    {
        *ppHead  = p;
        p->pNext = NULL;
        return;
    }
    else
    {
        while(pCur->pNext != NULL)
            pCur = pCur->pNext;

        pCur->pNext = p;
        p->pNext    = NULL;

        return;
    }
}

static int InterAllocBuffer(ATMContext* pContext)
{
    int   ePixelFormat;
    int   nBufHeight;
    int   nBufWidth;
    int   nMemSizeY;
    int   nMemSizeC;
    int   i;
    VideoPicture* pPicture;

    ATMConfig* pConfig = &pContext->mConfig;
    PicNode* pFrame = pContext->pFrameArray;

    ePixelFormat   = pConfig->nPixelFormat;
    nBufWidth = ATM_ALIGN(pConfig->nPicWidth, pConfig->nAlignSize);
    nBufHeight = ATM_ALIGN(pConfig->nPicHeight, pConfig->nAlignSize);
    logd("alloc buf width:%d height:%d", nBufWidth, nBufHeight);
    for(i = 0; i < pContext->mConfig.nFrameNum; i++)
    {
        pPicture = &(pFrame[i].vPicture);
        switch(ePixelFormat)
        {
            case PIXEL_FORMAT_YUV_PLANER_420:
            case PIXEL_FORMAT_YV12:
                nMemSizeY = nBufWidth * nBufHeight;
                nMemSizeC = nMemSizeY / 2;
                pPicture->pData0 = CdcMemPalloc(pContext->pMemOps, nMemSizeY + nMemSizeC, NULL, NULL);
                pPicture->pData1 = pPicture->pData0 + nMemSizeY;
                pPicture->pData2 = pPicture->pData1 + nMemSizeC / 2;
                break;
            case PIXEL_FORMAT_NV21:
            case PIXEL_FORMAT_NV12:
                nMemSizeY = nBufWidth * nBufHeight;
                nMemSizeC = nMemSizeY / 2;
                pPicture->pData0 = CdcMemPalloc(pContext->pMemOps, nMemSizeY + nMemSizeC, NULL, NULL);
                pPicture->pData1 = pPicture->pData0 + nMemSizeY;
                break;
            default:
                loge("pixel format not support");
                return -1;
        }
        pPicture->ePixelFormat = ePixelFormat;
        pPicture->nWidth = pConfig->nPicWidth;
        pPicture->nHeight = pConfig->nPicHeight;
        pPicture->nBufSize = nMemSizeY + nMemSizeC;
        pPicture->phyYBufAddr = (u32)CdcMemGetPhysicAddressCpu(pContext->pMemOps, pPicture->pData0);
        pPicture->phyCBufAddr = pPicture->phyYBufAddr + nMemSizeY;
        pPicture->nID = i;
        logd("alloc inter buffer idx:%d num:%d", i, pContext->mConfig.nFrameNum);
        InterEnqueue(&pContext->pEmptyQueue, &(pFrame[i]));
        logd("pic y addr:0x%lx cb:0x%lx", (unsigned long)pPicture->phyYBufAddr, (unsigned long)pPicture->phyCBufAddr);
    }

    return 0;
}

ATMInstant ATMCreate(struct ScMemOpsS* pMemOps, VeOpsS* pVeOps, void* pVeOpsSelf)
{
    ATMContext* pContext;
    if(pMemOps == NULL || pVeOps == NULL)
    {
        loge("param is null");
        return NULL;
    }
    pContext = (ATMContext*)malloc(sizeof(ATMContext));
    if(pContext == NULL)
    {
        loge("memory alloc fail.");
        return NULL;
    }
    memset(pContext, 0, sizeof(ATMContext));

    pthread_mutex_init(&pContext->nMutex, NULL);
    pContext->pMemOps = pMemOps;
    pContext->pVeOps = pVeOps;
    pContext->pVeOpsSelf = pVeOpsSelf;
    return (ATMInstant)pContext;
}

ATMInstant ATMCreate2(void)
{

    VeOpsS* pVeOps;
    VeConfig mVeConfig;
    void* pVeOpsSelf;
    struct ScMemOpsS* pMemOps;

    pVeOps = GetVeOpsS(VE_OPS_TYPE_NORMAL);
    memset(&mVeConfig, 0, sizeof(VeConfig));
    mVeConfig.nDecoderFlag = 1;
    mVeConfig.bNotSetVeFreq = 1;

    pVeOpsSelf = CdcVeInit(pVeOps, &mVeConfig);

    if(pVeOpsSelf == NULL)
    {
        loge("init ve ops failed");
        return NULL;
    }

    pMemOps = MemAdapterGetOpsS();

    return ATMCreate(pMemOps, pVeOps, pVeOpsSelf);

}

int ATMInitialize(ATMInstant pInstant)
{
    ATMContext* pContext = (ATMContext*)pInstant;
    pthread_mutex_lock(&pContext->nMutex);
    pContext->pScaleDown = SDCreate(pContext->pVeOps, pContext->pVeOpsSelf, pContext->pMemOps);

    pthread_mutex_unlock(&pContext->nMutex);
    return 0;
}

int ATMDestory(ATMInstant pInstant)
{
    VideoPicture* pPicture;
    int i;
    ATMContext* pContext = (ATMContext*)pInstant;
    PicNode* pFrame = pContext->pFrameArray;

    pthread_mutex_lock(&pContext->nMutex);
    SDDestory(pContext->pScaleDown);
    if(pContext->mConfig.nInterBufFlag)
    {
        for(i=0; i<pContext->mConfig.nFrameNum; i++)
        {
            pPicture = &(pFrame[i].vPicture);
            CdcMemPfree(pContext->pMemOps, pPicture->pData0, NULL, NULL);
        }
    }
    free(pContext->pFrameArray);
    pthread_mutex_unlock(&pContext->nMutex);
    free(pInstant);
    return 0;
}

int ATMSetParam(ATMInstant pInstant, ATMParam* pParam)
{

    ATMContext* pContext = (ATMContext*)pInstant;
    if(pContext == NULL || pParam == NULL)
    {
        loge("param is null, init fail.");
        return -1;
    }

    pthread_mutex_lock(&pContext->nMutex);
    pContext->mConfig.nInterBufFlag = pParam->bInterAllocFlag;
    pContext->mConfig.pFbm = pParam->pFbm;
    pContext->mConfig.nPicWidth = pParam->nPicWidth;
    pContext->mConfig.nPicHeight = pParam->nPicHeight;
    pContext->mConfig.nAlignSize = pParam->nAlignSize;
    pContext->mConfig.nPixelFormat = pParam->nPixelFormat;
    pthread_mutex_unlock(&pContext->nMutex);
    return 0;
}

int ATMScaleDownPrepare(ATMInstant pInstant)
{
    SDParam nSDParam;
    ATMContext* pContext = (ATMContext*)pInstant;
    nSDParam.buf_size = pContext->mConfig.nPicWidth*6;
    return SDPrePare(pContext->pScaleDown, &nSDParam);
}

int ATMCreateFrameQueue(ATMInstant pInstant, int FrameNum)
{
    ATMContext* pContext = (ATMContext*)pInstant;
    pthread_mutex_lock(&pContext->nMutex);
    pContext->pFrameArray = (PicNode*)malloc(FrameNum * sizeof(PicNode));
    if(pContext->pFrameArray == NULL)
    {
        loge("frame array alloc fail, num:%d", FrameNum);
        pthread_mutex_unlock(&pContext->nMutex);
        return -1;
    }

    pContext->mConfig.nFrameNum = FrameNum;
    memset(pContext->pFrameArray, 0, FrameNum * sizeof(PicNode));

    if(pContext->mConfig.nInterBufFlag)
    {
        if(InterAllocBuffer(pContext) < 0)
        {
            loge("alloc inter buffer fail");
            free(pContext->pFrameArray);
            pthread_mutex_unlock(&pContext->nMutex);
            return -1;
        }
    }
    pthread_mutex_unlock(&pContext->nMutex);
    return 0;
}

int ATMSetFrameAddr(ATMInstant pInstant, VideoPicture* pPicture)
{
    PicNode* pPicNode = NULL;

    ATMContext* pContext = (ATMContext*)pInstant;
    if(pContext->pFrameArray == NULL || pContext->mConfig.nInterBufFlag)
    {
        loge("func use error, framequeue:%p interbuf:%d",pContext->pFrameArray, pContext->mConfig.nInterBufFlag);
        return -1;
    }
    if(pContext->nValidBufNum >= pContext->mConfig.nFrameNum)
    {
        loge("you set too much buffer");
        return -1;
    }

    pthread_mutex_lock(&pContext->nMutex);
    pPicNode = &pContext->pFrameArray[pContext->nValidBufNum];
    pPicNode->vPicture.pData0      = pPicture->pData0;
    pPicNode->vPicture.pData1      = pPicture->pData1;
    pPicNode->vPicture.pData2      = pPicture->pData2;
    pPicNode->vPicture.phyYBufAddr = pPicture->phyYBufAddr;
    pPicNode->vPicture.phyCBufAddr = pPicture->phyCBufAddr;
    pPicNode->vPicture.nBufFd      = pPicture->nBufFd;
    pContext->nValidBufNum++;

    InterEnqueue(&pContext->pEmptyQueue, pPicNode);
    pthread_mutex_unlock(&pContext->nMutex);
    return 0;
}

int ATMSetFrameNewAddr(ATMInstant pInstant, VideoPicture* pPicture)
{
    PicNode* pPicNode;
    ATMContext* pContext = (ATMContext*)pInstant;
    pPicNode = InterDequeue(&pContext->pEmptyQueue);
    if(pPicNode != NULL)
    {
        loge("empty queue no buffer");
        return -1;
    }
    pPicNode->vPicture.pData0      = pPicture->pData0;
    pPicNode->vPicture.pData1      = pPicture->pData1;
    pPicNode->vPicture.pData2      = pPicture->pData2;
    pPicNode->vPicture.phyYBufAddr = pPicture->phyYBufAddr;
    pPicNode->vPicture.phyCBufAddr = pPicture->phyCBufAddr;
    pPicNode->vPicture.nBufFd      = pPicture->nBufFd;

    InterEnqueue(&pContext->pEmptyQueue, pPicNode);
    return 0;
}

int ATMReset(ATMInstant pInstant)
{
    (void)(pInstant);
    return 0;
}

int ATMRequstPicture(ATMInstant pInstant, VideoPicture** pPicture)
{
    PicNode* pPicNode = NULL;
    ATMContext* pContext = (ATMContext*)pInstant;

    pthread_mutex_lock(&pContext->nMutex);
    pPicNode = InterDequeue(&pContext->pValidQueue);
    if(pPicNode == NULL)
    {
        pPicture = NULL;
        pthread_mutex_unlock(&pContext->nMutex);
        return -1;
    }
    pContext->nValidPicNum--;
    *pPicture = &(pPicNode->vPicture);
    logd("pic node:%p validpic:%d pic:%p", pPicNode, pContext->nValidPicNum, *pPicture);
    pthread_mutex_unlock(&pContext->nMutex);
    return 0;
}

int ATMReturnPicture(ATMInstant pInstant, VideoPicture* pPicture)
{
    PicNode* pPicNode = NULL;
    int index;
    ATMContext* pContext = (ATMContext*)pInstant;
    if(pContext == NULL || pPicture == NULL)
    {
        loge("param is null, init fail.");
        pthread_mutex_unlock(&pContext->nMutex);
        return -1;
    }
    pthread_mutex_lock(&pContext->nMutex);
    index = pPicture->nID;
    pPicNode = &(pContext->pFrameArray[index]);
    InterEnqueue(&pContext->pEmptyQueue, pPicNode);
    pContext->nEmptyPicNum++;
    pthread_mutex_unlock(&pContext->nMutex);
    return 0;
}

int ATMAfterTreat(ATMInstant pInstant)
{
    VideoPicture* pPicture = NULL;
    PicNode* pPicNode = NULL;
    ATMContext* pContext = (ATMContext*)pInstant;
    if(pContext == NULL || pContext->mConfig.pFbm == NULL)
    {
        loge("context or pbm is null");
        return -1;
    }

    pthread_mutex_lock(&pContext->nMutex);
    pPicture = FbmRequestPicture(pContext->mConfig.pFbm);

    if(pPicture == NULL)
    {
        pthread_mutex_unlock(&pContext->nMutex);
        return -1;
    }
    pPicNode = InterDequeue(&pContext->pEmptyQueue);
    if(pPicNode == NULL)
    {
        pthread_mutex_unlock(&pContext->nMutex);
        return -1;
    }
    pContext->nEmptyPicNum--;
    logd("pic node:%p", pPicNode);
    if(SDProcess(pContext->pScaleDown, pPicture, &(pPicNode->vPicture)) < 0)
    {
        InterEnqueue(&pContext->pEmptyQueue, pPicNode);
        pContext->nEmptyPicNum++;
        pthread_mutex_unlock(&pContext->nMutex);
        return -2;
    }
    InterEnqueue(&pContext->pValidQueue, pPicNode);
    pContext->nValidPicNum++;
    logd("valid num:%d", pContext->nValidPicNum);
    FbmReturnPicture(pContext->mConfig.pFbm, pPicture);
    pthread_mutex_unlock(&pContext->nMutex);
    return 0;
}
