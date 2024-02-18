/*
 * =====================================================================================
 *
 *       Filename:  aftertreatment.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2020年07月30日 14时34分44秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *        Company:
 *
 * =====================================================================================
 */
 #ifndef _AW_ATM_H_
 #define _AW_ATM_H_

#include "vbasetype.h"
#include "fbm.h"
#include "memoryAdapter.h"
#include "veAdapter.h"

typedef struct ATMParam {
    int nMaxFrameNum;
    int bInterAllocFlag;
    int nPixelFormat;
    int nAlignSize;
    int nPicWidth;
    int nPicHeight;
    Fbm* pFbm;
} ATMParam;

typedef void* ATMInstant;

ATMInstant ATMCreate(struct ScMemOpsS* pMemOps, VeOpsS* pVeOps, void* pVeOpsSelf);
ATMInstant ATMCreate2(void);
int ATMInitialize(ATMInstant pContext);
int ATMDestory(ATMInstant pContext);
int ATMSetParam(ATMInstant pContext, ATMParam* pParam);
int ATMSetFrameAddr(ATMInstant pContext, VideoPicture* pPicture);
int ATMSetFrameNewAddr(ATMInstant pContext, VideoPicture* pPicture);
int ATMScaleDownPrepare(ATMInstant pInstant);
int ATMCreateFrameQueue(ATMInstant pInstant, int FrameNum);
int ATMReset(ATMInstant pContext);
int ATMRequstPicture(ATMInstant pContext, VideoPicture** pPicture);
int ATMReturnPicture(ATMInstant pContext, VideoPicture* pPicture);
int ATMAfterTreat(ATMInstant pContext);

#endif
