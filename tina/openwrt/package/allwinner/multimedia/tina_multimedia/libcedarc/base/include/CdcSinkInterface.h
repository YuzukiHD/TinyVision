/*
 * =====================================================================================
 *
 *       Filename:  CdcFileSink.h
 *
 *    Description: for save bitstream or picture
 *
 *        Version:  1.0
 *        Created:  2020年10月22日 14时40分05秒
 *       Revision:  none
 *       Compiler:  gcc
 *      CodeStyle:  android
 *
 *         Author:  jilinglin
 *        Company:  allwinnertech.com
 *
 * =====================================================================================
 */
#ifndef DAV1D_OUTPUT_MUXER_H
#define DAV1D_OUTPUT_MUXER_H

#include "typedef.h"

#define CDC_SINK_TYPE_PIC 1
#define CDC_SINK_TYPE_BS 2
#define CDC_SINK_TYPE_TIME 3
#define CDC_SINK_TYPE_NORMAL 4

//******************************
//picture format support:
//1. yuv plane, data[0]=y data[1]=u or y data[2]=v or u
//2. yuv semi plane, data[0]=y data[1]=uv or vu
//3. rgb, argb/
//*******************************

typedef enum CdcSinkPicFormat {
    CDC_SINK_PIC_FORMAT_YU12,
    CDC_SINK_PIC_FORMAT_YV12,
    CDC_SINK_PIC_FORMAT_NV12,
    CDC_SINK_PIC_FORMAT_NV21,
    CDC_SINK_PIC_FORMAT_ARGB,
    CDC_SINK_PIC_FORMAT_RGB,
} CdcSinkPicFormat;

typedef struct CdcSinkPicture {
    void* pData[4];
    int   nWidth;
    int   nHeight;
    int   nAlign;
    int   nBpp; //byte per pixle
    int   nAfbc;
    int   nAfbcSize;
    int   nTop;
    int   nBottom;
    int   nLeft;
    int   nRight;
    CdcSinkPicFormat nFormat;
} CdcSinkPicture;

typedef struct CdcBSHeader {
    char mMask[4];
    int  mBSIndex;
    int  mLen;
    u64  mPts;
} CdcBSHeader;

typedef struct CdcBitStream {
    void* pData;
    int   nLen;
    int   nAddHead;
    CdcBSHeader mHead;
} CdcBitStream;

typedef struct CdcSinkParam {
    //for all
    const char* pFileName;
    //for pic sink
    int   nLen;
    int   nStartNum;
    int   nToatalNum;
    int   bMd5Open;
} CdcSinkParam;

typedef struct CdcSinkInterface {
    //to picture
    int (*pWritePicture)(struct CdcSinkInterface *pSelf, CdcSinkPicture *pPic);
    int (*pWriteSinglePicture)(struct CdcSinkInterface *pSelf, CdcSinkPicture *pPic);
    //to bitstream
    int (*pWriteBitStream)(struct CdcSinkInterface *pSelf, CdcBitStream* pStream);
    int (*pSetParameter)(struct CdcSinkInterface *pSelf, CdcSinkParam *pParam);
} CdcSinkInterface;

static inline int CdcSinkWritePic(struct CdcSinkInterface *pSelf, CdcSinkPicture *pPic)
{
        return pSelf->pWritePicture(pSelf, pPic);
}

static inline int CdcSinkWriteSinglePic(struct CdcSinkInterface *pSelf, CdcSinkPicture *pPic)
{
        return pSelf->pWriteSinglePicture(pSelf, pPic);
}

static inline int CdcSinkWriteBSData(struct CdcSinkInterface *pSelf, CdcBitStream* pStream)
{
        return pSelf->pWriteBitStream(pSelf, pStream);
}

static inline int CdcSinkSetParam(struct CdcSinkInterface *pSelf, CdcSinkParam *pParam)
{
        return pSelf->pSetParameter(pSelf, pParam);
}

CdcSinkInterface* CdcSinkIFCreate(int nType);

void CdcSinkIFDestory(CdcSinkInterface* pSelf, int nType);

#endif /* DAV1D_OUTPUT_MUXER_H */
