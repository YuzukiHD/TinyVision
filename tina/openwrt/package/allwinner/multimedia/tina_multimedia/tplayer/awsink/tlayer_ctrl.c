/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : layerControl_de.cpp
* Description : Display2
* History :
*/

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <pthread.h>
#include "cdx_config.h"
#include <cdx_log.h>
#include "layerControl.h"
#include <ion_mem_alloc.h>
#include <videoOutPort.h>
#include "iniparserapi.h"
#include "tdisp_ctrl.h"
#include "sunxi_tr.h"

#define G2D_ENBLE 1
#if G2D_ENBLE
#include "tg2d_driver.h"
#endif
#define SAVE_PIC (0)

#define GPU_BUFFER_NUM 32

//static VideoPicture* gLastPicture = NULL;

#define BUF_MANAGE (0)

//#define ROTATE_BUFFER_NUM GetConfigParamterInt("pic_4rotate_num", 3)
#define ROTATE_BUFFER_NUM 3
#define TRANSFORM_DEV_TIMEOUT 200
#define ALIGN_32B(x) (((x) + (31)) & ~(31))
#define ALIGN_16B(x) (((x) + (15)) & ~(15))

#define HOLD_LAST_PICTURE_WITH (GetConfigParamterInt("hold_last_picture_with", 1920))
#define HOLD_LAST_PICTURE_HEIGHT (GetConfigParamterInt("hold_last_picture_height", 1088))

int LayerCtrlHideVideo(LayerCtrl* l);

int init_mutex_count = 0;
pthread_mutex_t configMtx;
uintptr_t ve_offset = 0;

typedef struct VPictureNode_t VPictureNode;
struct VPictureNode_t
{
    VideoPicture* pPicture;
    VPictureNode* pNext;
    int           bUsed;
};

typedef struct BufferInfoS
{
    VideoPicture pPicture;
    int          nUsedFlag;
}BufferInfoT;

typedef enum {
    NORMAL_PICTURE_BUF      = 0x00,
    LAST_PICTURE_BUF            = 0x01,
} LayerBufMode;

typedef struct LayerContext
{
	LayerCtrl			base;
	dispOutPort			*mDispOutPort;
	enum EPIXELFORMAT	eDisplayPixelFormat;
	int					nWidth;
	int					nHeight;
	int					nLeftOff;
	int					nTopOff;
	int					nDisplayWidth;
	int					nDisplayHeight;
	int					bHoldLastPictureFlag;
	int					bVideoWithTwoStreamFlag;
	int					bIsSoftDecoderFlag;
	int					bLayerInitialized;
	int					bProtectFlag;
	int					setSrcRectFlag;
	int					setDstRectFlag;
	VoutRect			src_rect;
	VoutRect			dst_rect;
    struct SunxiMemOpsS *pMemops;

    //* use when render derect to hardware layer.
    VPictureNode*        pPictureListHeader;
    VPictureNode         picNodes[16];

    int                  nGpuBufferCount;
    BufferInfoT          mBufferInfo[GPU_BUFFER_NUM];
    int                  bLayerShowed;

    int                  fdDisplay;
    int                  nScreenWidth;
    int                  nScreenHeight;
    VideoFrameCallback mVideoframeCallback;
    void*                pUserData;

    BufferInfoT    mRotateBufInfo[ROTATE_BUFFER_NUM];
    BufferInfoT    mRotateExtraBufInfo;
    int          mTransformFd;
    int          mInitedTransformFlag;
    unsigned long          mTransformChannel;
    int          mCreateRotateBufFlag;
    int          mG2dRotateDegree;


    VideoPicture mTmpCurPicture;
    VideoPicture mHoldLastPicture;
    int mHoldLastPictureFlag;
    int initLastPictureBufFlag;
    char* mHoldLastPictureBuf;

    int mNumHoldByLayer;
    int          b10BitPicFlag;
    int          nLbcLossyComMod;//1:1.5x; 2:2x; 3:2.5x;
    unsigned int bIsLossy;       //lossy compression or not
    unsigned int bRcEn;          //compact storage or not
}LayerContext;

static unsigned int getLbcModBuferrSize(LayerContext* lc)
{
	// int nLbcBufferSize = 0;
	 int cmp_ratio = 0;
	 int seg_wth = 16;
	 int seg_hgt = 4;
	 int bit_depth = 8;
	 //int ALIGN = 128;
	 int seg_tar_bits;
	 int segline_tar_bits;
	 int y_mode_bits, c_mode_bits, y_data_bits, c_data_bits;
	 int info_buffer_size;
	 int frm_wth = lc->nWidth;
	 int frm_hgt = (lc->nHeight+3) &~ 3;
	 unsigned int bufferSize;

	 if(lc->b10BitPicFlag == 1)
	 {
	   bit_depth = 10;
	 }

	 if(lc->nLbcLossyComMod == 1)//1.5x
	 {
	   cmp_ratio = 666;
	 }
	 else if(lc->nLbcLossyComMod == 2)//2x
	 {
	  cmp_ratio = 500;
	 }
	 else if(lc->nLbcLossyComMod == 3)//2.5x
	 {
	   cmp_ratio = 400;
	 }

	 if(lc->bIsLossy)
	 {
		 seg_tar_bits = ((seg_wth * seg_hgt * bit_depth * cmp_ratio * 3 / 2000) / ALIGN) * ALIGN;
		 if(lc->bRcEn)
		 {
		    segline_tar_bits = ((frm_wth + seg_wth - 1) / seg_wth * seg_wth * seg_hgt * bit_depth * cmp_ratio * 3 / 2000 + ALIGN - 1) / ALIGN * ALIGN;
		 }
		 else
		 {
		    segline_tar_bits = ((frm_wth + seg_wth - 1) / seg_wth) * seg_tar_bits;
		 }
	 }
	 else
	 {
		 y_mode_bits = seg_wth / MB_WTH * (CODE_MODE_Y_BITS * 2 + BLK_MODE_BITS);
		 c_mode_bits = 2 * (seg_wth / 2 / MB_WTH * CODE_MODE_C_BITS);
		 y_data_bits = seg_wth * seg_hgt * bit_depth;
		 c_data_bits = seg_wth * seg_hgt * bit_depth / 2 + 2 * (seg_wth / 2 / MB_WTH) * C_DTS_BITS;
		 seg_tar_bits= (y_data_bits + c_data_bits + y_mode_bits + c_mode_bits + ALIGN - 1) / ALIGN * ALIGN;
		 segline_tar_bits = ((frm_wth + seg_wth - 1) / seg_wth * seg_wth / seg_wth) * seg_tar_bits;
	 }

	 info_buffer_size = ((((frm_wth + seg_wth - 1) / seg_wth + 7) / 8) * 8 * 16 * (frm_hgt /seg_hgt) + 7) / 8;
	 bufferSize = (segline_tar_bits * frm_hgt / seg_hgt + 7) /8 + info_buffer_size;
	 logd("the_lbc:cmp_ratio=%d, buffer size=%d", cmp_ratio, bufferSize);

	 return bufferSize;
}


/* set usage, scaling_mode, pixelFormat, buffers_geometry, buffers_count, crop */
static int setLayerBuffer(LayerContext* lc,LayerBufMode mode)
{
    int          pixelFormat;
    unsigned int nGpuBufWidth;
    unsigned int nGpuBufHeight;
    unsigned int nGpuBufWidthAlign32;
    unsigned int nGpuBufHeightAlign64;
    int i = 0;
    char* pVirBuf;
    char* pPhyBuf;
    char* tmpPhyBuf;
	const char *vd_platform;
    int nBufferFd = 0;
    logd("%s:Fmt(%d),(%d %d, %d x %d)",
			__FUNCTION__, lc->eDisplayPixelFormat, lc->nWidth,
			lc->nHeight, lc->nLeftOff, lc->nTopOff);
    logd("Disp(%dx%d)buf_cnt(%d),ProFlag(%d),SoftDecFlag(%d)",
			lc->nDisplayWidth, lc->nDisplayHeight, lc->nGpuBufferCount,
			lc->bProtectFlag, lc->bIsSoftDecoderFlag);
    nGpuBufWidth  = lc->nWidth;
    nGpuBufHeight = lc->nHeight;
    nGpuBufWidthAlign32 = ((nGpuBufWidth + 31) & ~31);
    nGpuBufHeightAlign64 = ((nGpuBufHeight + 63) & ~63);

    /* We should double the height if the video with two stream,
     * so the nativeWindow will malloc double buffer
     */
    if(lc->bVideoWithTwoStreamFlag == 1)
    {
        nGpuBufHeight = 2*nGpuBufHeight;
    }

    if(lc->nGpuBufferCount <= 0)
    {
        printf("error: the lc->nGpuBufferCount[%d] is invalid!",lc->nGpuBufferCount);
        return -1;
    }

    vd_platform = GetConfigParamterString("platform", NULL);
    logv("platform NULL, pls check your config.");
    /*
        note:
        if(ic_version == 0x1639)
            ve_offset = 0x20000000;
        else if(ic_version <= 0x1718)
            ve_offset = 0x40000000;
         else
            ve_offset = 0;
    */
    if (strcmp(vd_platform, "r40") == 0) {//0x1701
        ve_offset = 0x40000000;
    }
    else if (strcmp(vd_platform, "r16") == 0) {//0x1667
        ve_offset = 0x40000000;
    }
    else if (strcmp(vd_platform, "r11") == 0) {//0x1681
        ve_offset = 0x40000000;
    }
    else if (strcmp(vd_platform, "r7") == 0) {//0x1681
        ve_offset = 0x40000000;
    }
    else if (strcmp(vd_platform, "r333") == 0) {//0x1681
        ve_offset = 0x40000000;
    }
    else if (strcmp(vd_platform, "331") == 0) {//0x1681
        ve_offset = 0x40000000;
    }
    else if (strcmp(vd_platform, "r7s") == 0) {//0x1681
        ve_offset = 0x40000000;
    }
    else if (strcmp(vd_platform, "332") == 0) {//0x1681
        ve_offset = 0x40000000;
    }
    else if (strcmp(vd_platform, "v306") == 0) {//0x1681
        ve_offset = 0x40000000;
    }
    else if (strcmp(vd_platform, "g102") == 0) {//0x1699,no ve
        ve_offset = 0x40000000;
    }
    else if (strcmp(vd_platform, "r58") == 0) {//0x1673
        ve_offset = 0x40000000;
    }
    else if (strcmp(vd_platform, "r6") == 0) {//0x1663
        ve_offset = 0x40000000;
    }else if (strcmp(vd_platform, "c200s") == 0) {//0x1663
        ve_offset = 0x40000000;
    }else if (strcmp(vd_platform, "r30") == 0) {//0x1719
        ve_offset = 0;
    }else if (strcmp(vd_platform, "r18") == 0) {//0x1689
        ve_offset = 0x40000000;
    }else if (strcmp(vd_platform, "r311") == 0) {//0x1755
        ve_offset = 0;
    }else if (strcmp(vd_platform, "t7") == 0) {//0x1708
        ve_offset = 0x40000000;
    }else if (strcmp(vd_platform, "mr133") == 0) {//0x1755
        ve_offset = 0;
    } else if (strcmp(vd_platform, "mr813") == 0) {//0x1855
        ve_offset = 0;
    }else if (strcmp(vd_platform, "r328s2") == 0) {//0x1821,no ve
        ve_offset = 0;
    } else if (strcmp(vd_platform, "r328s3") == 0) {//0x1821,no ve
        ve_offset = 0;
    } else if (strcmp(vd_platform, "r329") == 0) {//0x1851,no ve
        ve_offset = 0;
    } else if (strcmp(vd_platform, "a33i") == 0) {//0x1667
        ve_offset = 0x40000000;
    } else if (strcmp(vd_platform, "r528") == 0) {//0x1859
        ve_offset = 0;
    } else if (strcmp(vd_platform, "r528rv") == 0) {//0x1859
        ve_offset = 0;
    } else if (strcmp(vd_platform, "r528s2") == 0) {//0x1859
        ve_offset = 0;
    } else if (strcmp(vd_platform, "t113") == 0) {//0x1859
        ve_offset = 0;
    } else if (strcmp(vd_platform, "f133") == 0) {//0x1859
        ve_offset = 0;    
	} else if (strcmp(vd_platform, "f133-a") == 0) {//0x1859
        ve_offset = 0;    
	} else if (strcmp(vd_platform, "f133-b") == 0) {//0x1859
        ve_offset = 0;
    } else if (strcmp(vd_platform, "d1s") == 0) {//0x1859
        ve_offset = 0;
    } else if (strcmp(vd_platform, "d1-h") == 0) {//0x1859
        ve_offset = 0;
    } else if (strcmp(vd_platform, "v853") == 0) {//0x1886
        ve_offset = 0;
    }
    else {
        loge("invalid config '%s', pls check your cedarx.conf.",
                         vd_platform);
        ve_offset = 0;
    }

    if(lc->mHoldLastPictureFlag){
        if(!lc->initLastPictureBufFlag){
            lc->mHoldLastPictureBuf = (char*)SunxiMemPalloc(lc->pMemops, HOLD_LAST_PICTURE_WITH*HOLD_LAST_PICTURE_HEIGHT+ HOLD_LAST_PICTURE_WITH*HOLD_LAST_PICTURE_HEIGHT/2);
             if(lc->mHoldLastPictureBuf == NULL)
                loge("SunxiMemPalloc mHoldLastPictureBuf fail,with = %d,height = %d",HOLD_LAST_PICTURE_WITH,HOLD_LAST_PICTURE_HEIGHT);
             else{
                logd("SunxiMemPalloc mHoldLastPictureBuf successfully,with = %d,height = %d",HOLD_LAST_PICTURE_WITH,HOLD_LAST_PICTURE_HEIGHT);
        #ifdef CONF_KERNEL_IOMMU
                lc->mHoldLastPicture.nBufFd = SunxiMemGetBufferFd(lc->pMemops,lc->mHoldLastPictureBuf);
        #else
                lc->mHoldLastPicture.nBufFd = -1;
        #endif
                tmpPhyBuf = (char*)SunxiMemGetPhysicAddressCpu(lc->pMemops, lc->mHoldLastPictureBuf);
                lc->mHoldLastPicture.phyYBufAddr = tmpPhyBuf - ve_offset;
             }

             lc->initLastPictureBufFlag = 1;
        }
    }

    if(mode == NORMAL_PICTURE_BUF){
    //logd("setLayerBuffer:lc->eDisplayPixelFormat = %d,nGpuBufferCount = %d",lc->eDisplayPixelFormat,lc->nGpuBufferCount);
	int ret = 0;
    for(i=0; i<lc->nGpuBufferCount; i++)
    {
        if(lc->eDisplayPixelFormat == PIXEL_FORMAT_YUV_MB32_420)
        {
            pVirBuf = (char*)SunxiMemPalloc(lc->pMemops, nGpuBufWidthAlign32 * nGpuBufHeight + nGpuBufWidth*nGpuBufHeightAlign64/2);
        }
        else
        {
            unsigned int bufferSize = nGpuBufWidth * nGpuBufHeight * 3/2;
            if(lc->nLbcLossyComMod > 0)
            {
                bufferSize = getLbcModBuferrSize(lc);
                lc->mBufferInfo[i].pPicture.nLbcSize = bufferSize;
            }
            pVirBuf = (char*)SunxiMemPalloc(lc->pMemops, bufferSize);
        }
        if(pVirBuf == NULL){
           loge("SunxiMemPalloc err");
            assert(pVirBuf == NULL);
            ret = -1;
            break;
        }
        logd("SunxiMemPalloc buf[%d]:%p",i,pVirBuf);
#ifdef CONF_KERNEL_IOMMU
        nBufferFd = SunxiMemGetBufferFd(lc->pMemops,pVirBuf);
#else
        tmpPhyBuf = (char*)SunxiMemGetPhysicAddressCpu(lc->pMemops, pVirBuf);
        pPhyBuf =  tmpPhyBuf - ve_offset;
#endif
        lc->mBufferInfo[i].nUsedFlag    = 0;
        //logd("mBufferInfo[%d].pPicture = %p,pVirBuf = %p,tmpPhyBuf = %p,pPhyBuf = %p,ve_offset = 0x%x",i,&lc->mBufferInfo[i].pPicture,pVirBuf,tmpPhyBuf,pPhyBuf,ve_offset);
        lc->mBufferInfo[i].pPicture.nWidth  = lc->nWidth;
        lc->mBufferInfo[i].pPicture.nHeight = lc->nHeight;
        if(lc->eDisplayPixelFormat == PIXEL_FORMAT_YUV_MB32_420)
            lc->mBufferInfo[i].pPicture.nLineStride  = nGpuBufWidthAlign32;
        else
            lc->mBufferInfo[i].pPicture.nLineStride  = lc->nWidth;
        lc->mBufferInfo[i].pPicture.pData0       = pVirBuf;
        if(lc->eDisplayPixelFormat == PIXEL_FORMAT_YUV_MB32_420)
        {
            lc->mBufferInfo[i].pPicture.pData1       = pVirBuf + (nGpuBufWidthAlign32 * nGpuBufHeight);
            lc->mBufferInfo[i].pPicture.pData2       = NULL;
        }
        else
        {
            lc->mBufferInfo[i].pPicture.pData1       = pVirBuf + (lc->nHeight * lc->nWidth);
            lc->mBufferInfo[i].pPicture.pData2       =
            lc->mBufferInfo[i].pPicture.pData1 + (lc->nHeight * lc->nWidth)/4;
        }
#ifdef CONF_KERNEL_IOMMU
        lc->mBufferInfo[i].pPicture.nBufFd   = nBufferFd;
#else
		lc->mBufferInfo[i].pPicture.nBufFd = -1;
        lc->mBufferInfo[i].pPicture.phyYBufAddr  = (uintptr_t)pPhyBuf;
        if(lc->eDisplayPixelFormat == PIXEL_FORMAT_YUV_MB32_420)
        {
            lc->mBufferInfo[i].pPicture.phyCBufAddr  =
                lc->mBufferInfo[i].pPicture.phyYBufAddr + (nGpuBufWidthAlign32 * nGpuBufHeight);
        }
        else
        {
            lc->mBufferInfo[i].pPicture.phyCBufAddr  =
                lc->mBufferInfo[i].pPicture.phyYBufAddr + (lc->nHeight * lc->nWidth);
        }
#endif
        if(lc->nLbcLossyComMod > 0)
        {
            lc->mBufferInfo[i].pPicture.pData1 = NULL;
            lc->mBufferInfo[i].pPicture.pData2 = NULL;
            lc->mBufferInfo[i].pPicture.phyCBufAddr = 0;
        }
        lc->mBufferInfo[i].pPicture.nBufId   = i;
        lc->mBufferInfo[i].pPicture.ePixelFormat = lc->eDisplayPixelFormat;
    }
        /*if has err,we should free the palloced buf*/
        if(ret == -1){
             int j = 0;
             for(j = 0;j < i;j++){
                SunxiMemPfree(lc->pMemops, lc->mBufferInfo[j].pPicture.pData0);
                lc->mBufferInfo[j].pPicture.pData0 = NULL;
             }
             return -1;
        }
	}else if(mode == LAST_PICTURE_BUF){
            nGpuBufWidth  = lc->mTmpCurPicture.nWidth;
            nGpuBufHeight = lc->mTmpCurPicture.nHeight;
            nGpuBufWidthAlign32 = ((nGpuBufWidth + 31) & ~31);
            nGpuBufHeightAlign64 = ((nGpuBufHeight + 63) & ~63);
            uintptr_t tmpStoreHoldLastPicturePhyY = lc->mHoldLastPicture.phyYBufAddr;
            uintptr_t tmpStoreHoldLastPicturePhyC = 0;
            int tmpStoreHoldLastPictureFd = lc->mHoldLastPicture.nBufFd;
            if(lc->eDisplayPixelFormat == PIXEL_FORMAT_YUV_MB32_420)
                tmpStoreHoldLastPicturePhyC = lc->mHoldLastPicture.phyYBufAddr + (nGpuBufWidthAlign32 * nGpuBufHeight);
            else
                tmpStoreHoldLastPicturePhyC = lc->mHoldLastPicture.phyYBufAddr + (nGpuBufWidth * nGpuBufHeight);
            memcpy(&lc->mHoldLastPicture,&lc->mTmpCurPicture,sizeof(VideoPicture));
            if(lc->eDisplayPixelFormat == PIXEL_FORMAT_YUV_MB32_420){
                 if(nGpuBufWidthAlign32<=HOLD_LAST_PICTURE_WITH && nGpuBufHeightAlign64<=HOLD_LAST_PICTURE_HEIGHT && lc->mHoldLastPictureBuf && lc->mTmpCurPicture.pData0){
                      memcpy(lc->mHoldLastPictureBuf ,lc->mTmpCurPicture.pData0,nGpuBufWidthAlign32 * nGpuBufHeight + nGpuBufWidth*nGpuBufHeightAlign64/2);
                      SunxiMemFlushCache(lc->pMemops, lc->mHoldLastPictureBuf , nGpuBufWidthAlign32 * nGpuBufHeight + nGpuBufWidth*nGpuBufHeightAlign64/2);
                      lc->mHoldLastPicture.pData0       = lc->mHoldLastPictureBuf;
                      lc->mHoldLastPicture.pData1       = lc->mHoldLastPictureBuf + (nGpuBufWidthAlign32 * nGpuBufHeight);
                      lc->mHoldLastPicture.pData2       = NULL;
                      lc->mHoldLastPicture.phyYBufAddr = tmpStoreHoldLastPicturePhyY;
                      lc->mHoldLastPicture.phyCBufAddr  = tmpStoreHoldLastPicturePhyC;
                      lc->mHoldLastPicture.nBufFd = tmpStoreHoldLastPictureFd;
                      return 0;
                 }
            }else{
                logd("last picture buf:%p,nGpuBufWidth = %d,nGpuBufHeight = %d",lc->mTmpCurPicture.pData0,nGpuBufWidth,nGpuBufHeight);
                if(nGpuBufWidth<=HOLD_LAST_PICTURE_WITH && nGpuBufHeight<=HOLD_LAST_PICTURE_HEIGHT && lc->mHoldLastPictureBuf && lc->mTmpCurPicture.pData0){
                    unsigned int bufferSize = nGpuBufWidth * nGpuBufHeight * 3/2;
                    if(lc->nLbcLossyComMod > 0)
                    {
                        bufferSize = getLbcModBuferrSize(lc);
                    }
                    memcpy(lc->mHoldLastPictureBuf,lc->mTmpCurPicture.pData0,bufferSize);
                    SunxiMemFlushCache(lc->pMemops, lc->mHoldLastPictureBuf, bufferSize);
                    lc->mHoldLastPicture.pData0       = lc->mHoldLastPictureBuf;
                    lc->mHoldLastPicture.pData1       = lc->mHoldLastPictureBuf + (lc->nHeight * lc->nWidth);
                    lc->mHoldLastPicture.pData2       = lc->mHoldLastPicture.pData1 + (lc->nHeight * lc->nWidth)/4;
                    lc->mHoldLastPicture.phyYBufAddr = tmpStoreHoldLastPicturePhyY;
                    lc->mHoldLastPicture.phyCBufAddr  = tmpStoreHoldLastPicturePhyC;
                    lc->mHoldLastPicture.nBufFd = tmpStoreHoldLastPictureFd;
                    return 0;
                }
            }
            return -1;
        }
    return 0;
}
static int SetLayerParam(LayerContext* lc, VideoPicture* pPicture,LayerBufMode mode)
{
	unsigned int args[4];
	unsigned long fbAddr[3];
	videoParam vparam;
	renderBuf rBuf;
    int cropHeight;
    int cropWidth;
    int cropStartX;
    int cropStartY;

    //* close the layer first, otherwise, in case when last frame is kept showing,
    //* the picture showed will not valid because parameters changed.
    if(lc->bLayerShowed == 1)
    {
        lc->bLayerShowed = 0;
    }
    memcpy(&lc->mTmpCurPicture,pPicture,sizeof(VideoPicture));//store the current picture info
    //fbAddr[0] = (unsigned long)SunxiMemGetPhysicAddressCpu(lc->pMemops,pPicture->pData0);
    fbAddr[0] = pPicture->phyYBufAddr+ve_offset;
    //fbAddr[1] = (unsigned long)SunxiMemGetPhysicAddressCpu(lc->pMemops,pPicture->pData1);
    fbAddr[1] = pPicture->phyCBufAddr+ve_offset;
    //fbAddr[2] = (unsigned long)SunxiMemGetPhysicAddressCpu(lc->pMemops,pPicture->pData2);
    fbAddr[2] = fbAddr[1]+(pPicture->nWidth*pPicture->nHeight)/4;

    //Use G2D rotation 90°/180°, (x,y) point offsets need to be calculated.
    cropWidth = pPicture->nRightOffset - pPicture->nLeftOffset;
    cropHeight = pPicture->nBottomOffset - pPicture->nTopOffset;
    if (90 == lc->mG2dRotateDegree || 180 == lc->mG2dRotateDegree)
    {
        cropStartX = pPicture->nWidth - cropWidth;
        cropStartY = pPicture->nHeight - cropHeight;
    }
    else
    {
        cropStartX = pPicture->nLeftOffset;
        cropStartY = pPicture->nTopOffset;
    }

	vparam.srcInfo.crop_x = cropStartX;
	vparam.srcInfo.crop_y = cropStartY;
	vparam.srcInfo.crop_w = cropWidth;
	vparam.srcInfo.crop_h = cropHeight;
    logd("crop_w = %d,crop_h = %d,nWidth = %d,nHeight = %d",vparam.srcInfo.crop_w,vparam.srcInfo.crop_h,pPicture->nWidth,pPicture->nHeight);
    if(vparam.srcInfo.crop_w  == 0)
        vparam.srcInfo.crop_w = pPicture->nWidth;
    if(vparam.srcInfo.crop_h  == 0)
        vparam.srcInfo.crop_h = pPicture->nHeight;
	vparam.srcInfo.w = pPicture->nWidth;
	vparam.srcInfo.h = pPicture->nHeight;
	vparam.srcInfo.format = pPicture->ePixelFormat;
	vparam.srcInfo.color_space = VIDEO_BT601;
	vparam.srcInfo.lbc_mod = pPicture->nLbcLossyComMod;
	vparam.srcInfo.is_lossy = pPicture->bIsLossy;
	vparam.srcInfo.rc_en = pPicture->bRcEn;
#ifdef CONF_KERNEL_IOMMU
	rBuf.fd = pPicture->nBufFd;
#endif
	rBuf.y_phaddr = fbAddr[0];
	rBuf.u_phaddr = fbAddr[1];
	rBuf.v_phaddr = fbAddr[2];
    rBuf.isExtPhy = VIDEO_USE_EXTERN_ION_BUF;
	pthread_mutex_lock(&configMtx);
    //logd("lc(%p):display picture = %p,pVirBuf = %p,pPhyBuf = %p",lc,pPicture,pPicture->pData0,fbAddr[0]);
	lc->mDispOutPort->queueToDisplay(lc->mDispOutPort,vparam.srcInfo.w*vparam.srcInfo.h*3/2, &vparam, &rBuf);
	lc->mDispOutPort->SetZorder(lc->mDispOutPort, VIDEO_ZORDER_BOTTOM);
	lc->mDispOutPort->setEnable(lc->mDispOutPort, 1);
	if(mode == LAST_PICTURE_BUF){
		/*If hold last picture,Immediately releasing the buffer may display anomalies
         * because the display layer has not been closed, 1000(ms)/60(fps)=17 */
		usleep(17 *1000);
	}
	pthread_mutex_unlock(&configMtx);

    return 0;
}

static int __LayerReset(LayerCtrl* l)
{
    LayerContext* lc;
    int i;

    logd("__LayerReset.");

    lc = (LayerContext*)l;

    for(i=0; i<lc->nGpuBufferCount; i++)
    {
        SunxiMemPfree(lc->pMemops, lc->mBufferInfo[i].pPicture.pData0);
    }

    return 0;
}

static void __LayerRelease(LayerCtrl* l)
{
    LayerContext* lc;
    int i;
    int j;

    lc = (LayerContext*)l;

    logd("__LayerRelease,lc->mHoldLastPictureFlag = %d,lc->nGpuBufferCount = %d,lc->mTmpCurPicture.nWidth = %d,lc->mTmpCurPicture.nHeight = %d",
      lc->mHoldLastPictureFlag,lc->nGpuBufferCount,lc->mTmpCurPicture.nWidth,lc->mTmpCurPicture.nHeight);
    if(lc->mHoldLastPictureFlag ){
        if((lc->mTmpCurPicture.nWidth* lc->mTmpCurPicture.nHeight) <= (HOLD_LAST_PICTURE_WITH *  HOLD_LAST_PICTURE_HEIGHT)){
            logd("keep last picture");
            setLayerBuffer(lc,LAST_PICTURE_BUF);
            SetLayerParam(lc, &lc->mHoldLastPicture,LAST_PICTURE_BUF);
        }else{
            logd("the buffer is not enough,do not keep last picture");
            lc->bLayerShowed = 0;
			pthread_mutex_lock(&configMtx);
            lc->mDispOutPort->setEnable(lc->mDispOutPort, 0);
			pthread_mutex_unlock(&configMtx);
        }
    }else{
            logd("disable disp out port before release framebuffer");
            lc->bLayerShowed = 0;
			pthread_mutex_lock(&configMtx);
            lc->mDispOutPort->setEnable(lc->mDispOutPort, 0);
			pthread_mutex_unlock(&configMtx);
    }
    for(i=0; i<lc->nGpuBufferCount; i++)
    {
        logd("free buf[%d] :%p",i,lc->mBufferInfo[i].pPicture.pData0);
        SunxiMemPfree(lc->pMemops, lc->mBufferInfo[i].pPicture.pData0);
    }
    if(GetConfigParamterInt("tr_rotate_flag",0) == 1 ||
			GetConfigParamterInt("g2d_rotate_flag",0) == 1 || lc->mG2dRotateDegree != 0){
        for(j = 0 ; j < ROTATE_BUFFER_NUM;j++){
            if(lc->mRotateBufInfo[j].pPicture.pData0){
                SunxiMemPfree(lc->pMemops, lc->mRotateBufInfo[j].pPicture.pData0);
                lc->mRotateBufInfo[j].pPicture.pData0 = NULL;
            }
        }
        if (lc->mRotateExtraBufInfo.pPicture.pData0 != NULL)
        {
            SunxiMemPfree(lc->pMemops, lc->mRotateExtraBufInfo.pPicture.pData0);
            lc->mRotateExtraBufInfo.pPicture.pData0 = NULL;
        }
        lc->mCreateRotateBufFlag = 0;
    }
}

static void __LayerDestroy(LayerCtrl* l)
{
    LayerContext* lc;
    int i;
    logd("__LayerDestroy");
    lc = (LayerContext*)l;
    if(lc->initLastPictureBufFlag){
        if(lc->mHoldLastPictureBuf){
			pthread_mutex_lock(&configMtx);
            lc->mDispOutPort->setEnable(lc->mDispOutPort, 0);
			pthread_mutex_unlock(&configMtx);
            SunxiMemPfree(lc->pMemops,lc->mHoldLastPictureBuf);
            lc->mHoldLastPictureBuf = NULL;
        }
    }
	pthread_mutex_lock(&configMtx);
    lc->mDispOutPort->deinit(lc->mDispOutPort);
	pthread_mutex_unlock(&configMtx);
    if(lc->pMemops)
        SunxiMemClose(lc->pMemops);
    logd("init_mutex_count = %d",init_mutex_count);
    init_mutex_count --;
    if(!init_mutex_count){
        pthread_mutex_destroy(&configMtx);
    }
    DestroyVideoOutport(lc->mDispOutPort);
    free(lc);
}

static int __LayerSetDisplayBufferSize(LayerCtrl* l, int nWidth, int nHeight)
{
    LayerContext* lc;

    lc = (LayerContext*)l;

    lc->nWidth         = nWidth;
    lc->nHeight        = nHeight;
    lc->nDisplayWidth  = nWidth;
    lc->nDisplayHeight = nHeight;
    lc->nLeftOff       = 0;
    lc->nTopOff        = 0;
    lc->bLayerInitialized = 0;
    logd("__LayerSetDisplayBufferSize:width = %d,height = %d",nWidth,nHeight);
    if(lc->bVideoWithTwoStreamFlag == 1)
    {
        /* display the whole buffer region when 3D
           * as we had init align-edge-region to black. so it will be look ok.
           */
        int nScaler = 2;
        lc->nDisplayHeight = lc->nDisplayHeight*nScaler;
    }

    return 0;
}

/* Description: set initial param -- display region */
static int __LayerSetDisplayRegion(LayerCtrl* l, int nLeftOff, int nTopOff,
                                        int nDisplayWidth, int nDisplayHeight)
{
    LayerContext* lc;

    lc = (LayerContext*)l;
    logv("Layer set display region,(%d %d, %dx%d)\n",
            nLeftOff, nTopOff, nDisplayWidth, nDisplayHeight);

    if(nDisplayWidth == 0 && nDisplayHeight == 0)
    {
        return -1;
    }

    lc->nDisplayWidth     = nDisplayWidth;
    lc->nDisplayHeight    = nDisplayHeight;
    lc->nLeftOff          = nLeftOff;
    lc->nTopOff           = nTopOff;

    if(lc->bVideoWithTwoStreamFlag == 1)
    {
        int nScaler = 2;
        lc->nDisplayHeight = lc->nHeight*nScaler;
    }

    return 0;
}

/* Description: set initial param -- display pixelFormat */
static int __LayerSetDisplayPixelFormat(LayerCtrl* l, enum EPIXELFORMAT ePixelFormat)
{
    LayerContext* lc;

    lc = (LayerContext*)l;
    logd("Layer set expected pixel format, format = %d", (int)ePixelFormat);

    if(ePixelFormat == PIXEL_FORMAT_NV12 ||
       ePixelFormat == PIXEL_FORMAT_NV21 ||
       ePixelFormat == PIXEL_FORMAT_YV12 ||
       ePixelFormat == PIXEL_FORMAT_YUV_MB32_420)
    {
        lc->eDisplayPixelFormat = ePixelFormat;
    }
    else
    {
        logv("receive pixel format is %d, not match.", lc->eDisplayPixelFormat);
        return -1;
    }

    return 0;
}

//* Description: set initial param -- deinterlace flag
static int __LayerSetDeinterlaceFlag(LayerCtrl* l,int bFlag)
{
    LayerContext* lc;
    (void)bFlag;
    lc = (LayerContext*)l;

    return 0;
}

//* Description: set buffer timestamp -- set this param every frame
static int __LayerSetBufferTimeStamp(LayerCtrl* l, int64_t nPtsAbs)
{
    LayerContext* lc;
    (void)nPtsAbs;

    lc = (LayerContext*)l;

    return 0;
}

static int __LayerGetRotationAngle(LayerCtrl* l)
{
    LayerContext* lc;
    int nRotationAngle = 0;

    lc = (LayerContext*)l;

    return 0;
}

static int __LayerCtrlShowVideo(LayerCtrl* l)
{
    LayerContext* lc;
    int i;

    lc = (LayerContext*)l;

    lc->bLayerShowed = 1;

    return 0;
}

static int __LayerCtrlHideVideo(LayerCtrl* l)
{
    LayerContext* lc;
    int i;

    lc = (LayerContext*)l;
    logd("__LayerCtrlHideVideo");
    lc->bLayerShowed = 0;
	pthread_mutex_lock(&configMtx);
    lc->mDispOutPort->setEnable(lc->mDispOutPort, 0);
	pthread_mutex_unlock(&configMtx);

    return 0;
}

static int __LayerCtrlIsVideoShow(LayerCtrl* l)
{
    LayerContext* lc;

    lc = (LayerContext*)l;
    logd("__LayerCtrlIsVideoShow");
     return lc->bLayerShowed;
}

static int  __LayerCtrlHoldLastPicture(LayerCtrl* l, int bHold)
{
    logd("LayerCtrlHoldLastPicture, bHold = %d", bHold);

    LayerContext* lc;
    lc = (LayerContext*)l;
    lc->mHoldLastPictureFlag = bHold;
    return 0;
}

static int __LayerDequeueBuffer(LayerCtrl* l, VideoPicture** ppVideoPicture, int bInitFlag)
{
    LayerContext* lc;
    int i = 0;
    VPictureNode*     nodePtr;
    BufferInfoT bufInfo;
    VideoPicture* pPicture = NULL;
    VPictureNode*     newNode;

    lc = (LayerContext*)l;

    if(lc->bLayerInitialized == 0)
    {
        if(setLayerBuffer(lc,NORMAL_PICTURE_BUF) != 0)
        {
            loge("can not initialize layer.");
            return -1;
        }

        lc->bLayerInitialized = 1;
    }

    if(bInitFlag == 1)
    {
        for(i = 0; i < lc->nGpuBufferCount; i++)
        {
            if(lc->mBufferInfo[i].nUsedFlag == 0)
            {
                //* set the buffer address
                pPicture = *ppVideoPicture;
                pPicture = &lc->mBufferInfo[i].pPicture;
                int numNotHoldByLayer = lc->nGpuBufferCount - lc->mNumHoldByLayer;
                logd("numNotHoldByLayer = %d,lc->nGpuBufferCount = %d",numNotHoldByLayer,lc->nGpuBufferCount);
                if(i >= numNotHoldByLayer){
                    //init the picture which hold by layer
                    int picNodeIndex = i-numNotHoldByLayer;
                    //logd("add newNode[%d] to list,pPicture->pData0 = %p,lc->mBufferInfo[i].pPicture.pData0 = %p",
                    //          picNodeIndex,pPicture->pData0,lc->mBufferInfo[i].pPicture.pData0);
                    if(lc->picNodes[picNodeIndex].bUsed == 0)
                    {
                        newNode = &lc->picNodes[picNodeIndex];
                        newNode->pNext = NULL;
                        newNode->bUsed = 1;
                        newNode->pPicture = &lc->mBufferInfo[i].pPicture;
                        if(lc->pPictureListHeader != NULL)
                        {
                            nodePtr = lc->pPictureListHeader;
                            while(nodePtr->pNext != NULL)
                            {
                                nodePtr = nodePtr->pNext;
                            }
                            nodePtr->pNext = newNode;
                        }
                        else
                        {
                            lc->pPictureListHeader = newNode;
                        }
                        //logd("add newNode[%d] to list,newNode->pPicture->pData0 =  %p",picNodeIndex,newNode->pPicture->pData0);
                    }
                }
                lc->mBufferInfo[i].nUsedFlag = 1;
                break;
            }
        }
    }
    else
    {
        if(lc->pPictureListHeader != NULL)
        {
            nodePtr = lc->pPictureListHeader;
            i=1;
            //logd("dequeue picture[%d] = %p,pVirBuf = %p,pPhyBuf = %p,nodePtr->bUsed = %d",
            //      i,nodePtr->pPicture,nodePtr->pPicture->pData0,nodePtr->pPicture->phyYBufAddr,nodePtr->bUsed);
            while(nodePtr->pNext != NULL)
            {
                i++;
                nodePtr = nodePtr->pNext;
            }
            //logd("has %d picture in layer",i);
            //* return one picture.
            if(i > GetConfigParamterInt("pic_4list_num", 3))
            {
                nodePtr = lc->pPictureListHeader;
                lc->pPictureListHeader = lc->pPictureListHeader->pNext;
                pPicture = nodePtr->pPicture;
                nodePtr->bUsed = 0;
                i--;
            }
        }
    }
    *ppVideoPicture = pPicture;
    if(pPicture == NULL)
        return -1;
    return 0;
}

// this method should block here,
static int __LayerQueueBuffer(LayerCtrl* l, VideoPicture* pBuf, int bValid)
{
    LayerContext* lc  = NULL;
    int               i;
    VPictureNode*     newNode;
    VPictureNode*     nodePtr;
    BufferInfoT    bufInfo;
    int  index = 0;
    int rotateDegree = 0;
    lc = (LayerContext*)l;
    lc->mVideoframeCallback(lc->pUserData,pBuf);
    if(lc->bLayerInitialized == 0)
    {
        logd("LayerQueueBuffer:setLayerBuffer");
        if(setLayerBuffer(lc,NORMAL_PICTURE_BUF) != 0)
        {
            loge("can not initialize layer.");
            return -1;
        }
        lc->bLayerInitialized = 1;
    }
    if(bValid == 1){
        if(GetConfigParamterInt("tr_rotate_flag",0) == 1){
            rotateDegree = GetConfigParamterInt("tr_rotate_degree",0);
            if(lc->mCreateRotateBufFlag == 0){
                if(rotateDegree == 90 || rotateDegree == 270){
                    createRotateBuf( l,pBuf->nHeight,pBuf->nWidth);
                    lc->base.ops->setDisplayRegion(l,0,0,pBuf->nHeight,pBuf->nWidth);
                }else if(rotateDegree == 0 || rotateDegree == 180){
                    createRotateBuf( l,pBuf->nWidth,pBuf->nHeight);
                    lc->base.ops->setDisplayRegion(l,0,0,pBuf->nWidth,pBuf->nHeight);
                }else{
                    logw("the rotateDegree = %d,which is not support,set it to 90",rotateDegree);
                    createRotateBuf( l,pBuf->nHeight,pBuf->nWidth);
                    lc->base.ops->setDisplayRegion(l,0,0,pBuf->nHeight,pBuf->nWidth);
                }
                lc->mCreateRotateBufFlag = 1;
            }
            static long long count = 0;
            index = count % ROTATE_BUFFER_NUM;
            hwRotateVideoPicture(l,pBuf,&lc->mRotateBufInfo[index].pPicture,rotateDegree);
            count++;
            if(SetLayerParam(lc, &lc->mRotateBufInfo[index].pPicture,NORMAL_PICTURE_BUF) != 0)
            {
                loge("can not initialize layer.");
            }
        }
	#if G2D_ENBLE
		else if(lc->mG2dRotateDegree != 0 || GetConfigParamterInt("g2d_rotate_flag",0) == 1)
		{
            rotateDegree = GetConfigParamterInt("g2d_rotate_degree",0);
            if (rotateDegree == 0)
            {
                rotateDegree = lc->mG2dRotateDegree;
            }else
            {
                lc->mG2dRotateDegree = rotateDegree;
            }

            if(lc->mCreateRotateBufFlag == 0){
                if(rotateDegree == 90 || rotateDegree == 270){
                    createG2dRotateBuf( l,pBuf,rotateDegree);
                    lc->base.ops->setDisplayRegion(l,0,0,pBuf->nHeight,pBuf->nWidth);
					lc->mCreateRotateBufFlag = 1;
                }else if(rotateDegree == 1000 || rotateDegree == 2000  || rotateDegree == 180){
                    createG2dRotateBuf( l,pBuf,rotateDegree);
                    lc->base.ops->setDisplayRegion(l,0,0,pBuf->nWidth,pBuf->nHeight);
					lc->mCreateRotateBufFlag = 1;
                }else{
                    logw("the rotateDegree = %d,which is not support,set it to 0",rotateDegree);
                   // createG2dRotateBuf( l,pBuf,rotateDegree);
                    lc->base.ops->setDisplayRegion(l,0,0,pBuf->nWidth,pBuf->nHeight);
				   lc->mCreateRotateBufFlag = 0;
                }
            }
            static long long count = 0;
            index = count % ROTATE_BUFFER_NUM;
			if( lc->mCreateRotateBufFlag == 1)
			{
                hwG2dRotateVideoPicture(l,pBuf,&lc->mRotateBufInfo[index].pPicture,rotateDegree);
	            if(SetLayerParam(lc, &lc->mRotateBufInfo[index].pPicture,NORMAL_PICTURE_BUF) != 0)
	            {
	                loge("can not initialize layer.");
	            }
			}
			else
			{
	            if(SetLayerParam(lc, pBuf,NORMAL_PICTURE_BUF) != 0)
	            {
	                loge("can not initialize layer.");
	            }
			}
            count++;
		}
	#endif
		else{
            lc->base.ops->setDisplayRegion(l,0,0,pBuf->nWidth,pBuf->nHeight);
            if(SetLayerParam(lc, pBuf,NORMAL_PICTURE_BUF) != 0)
            {
                loge("can not initialize layer.");
            }
        }
    }
/*queue the picture buf into list*/
    newNode = NULL;
    for(i = 0; i < lc->mNumHoldByLayer+1; i++)
    {
        if(lc->picNodes[i].bUsed == 0)
        {
            newNode = &lc->picNodes[i];
            newNode->pNext = NULL;
            newNode->bUsed = 1;
            newNode->pPicture = pBuf;
            break;
        }
    }
    if(i == lc->mNumHoldByLayer+1)
    {
        loge("*** picNode is full when queue buffer");
        return -1;
    }

    if(lc->pPictureListHeader != NULL)
    {
        nodePtr = lc->pPictureListHeader;
        while(nodePtr->pNext != NULL)
        {
            nodePtr = nodePtr->pNext;
        }
        nodePtr->pNext = newNode;
    }
    else
    {
        lc->pPictureListHeader = newNode;
    }

    return 0;
}

static int __LayerSetDisplayBufferCount(LayerCtrl* l, int nBufferCount)
{
    LayerContext* lc;

    lc = (LayerContext*)l;

    logd("LayerSetBufferCount: count = %d",nBufferCount);

    lc->nGpuBufferCount = nBufferCount;

    if(lc->nGpuBufferCount > GPU_BUFFER_NUM)
        lc->nGpuBufferCount = GPU_BUFFER_NUM;

    return lc->nGpuBufferCount;
}

static int __LayerGetBufferNumHoldByGpu(LayerCtrl* l)
{
    LayerContext* lc;
    lc = (LayerContext*)l;
    int holdNum = lc->mNumHoldByLayer;
    logd("num hold by gpu is %d",holdNum);
    return holdNum;
}

static int __LayerGetDisplayFPS(LayerCtrl* l)
{
    (void)l;
    return 60;
}

static void __LayerResetNativeWindow(LayerCtrl* l,void* pNativeWindow)
{
    logd("LayerResetNativeWindow : %p ",pNativeWindow);

    LayerContext* lc;
    VideoPicture mPicBufInfo;

    lc = (LayerContext*)l;
    lc->bLayerInitialized = 0;

    return ;
}

static VideoPicture* __LayerGetBufferOwnedByGpu(LayerCtrl* l)
{
    LayerContext* lc;
    VideoPicture* pPicture = NULL;
    BufferInfoT bufInfo;

    lc = (LayerContext*)l;
    int i;
    VPictureNode*     nodePtr;
    if(lc->pPictureListHeader != NULL)
    {
        nodePtr = lc->pPictureListHeader;
        lc->pPictureListHeader = lc->pPictureListHeader->pNext;
        pPicture = nodePtr->pPicture;
        nodePtr->bUsed = 0;
    }
    return pPicture;
}

static int __LayerSetVideoWithTwoStreamFlag(LayerCtrl* l, int bVideoWithTwoStreamFlag)
{
    LayerContext* lc;

    lc = (LayerContext*)l;

    logv("LayerSetIsTwoVideoStreamFlag, flag = %d",bVideoWithTwoStreamFlag);
    lc->bVideoWithTwoStreamFlag = bVideoWithTwoStreamFlag;

    return 0;
}

static int __LayerSetIsSoftDecoderFlag(LayerCtrl* l, int bIsSoftDecoderFlag)
{
    LayerContext* lc;

    lc = (LayerContext*)l;

    logv("LayerSetIsSoftDecoderFlag, flag = %d",bIsSoftDecoderFlag);
    lc->bIsSoftDecoderFlag = bIsSoftDecoderFlag;

    return 0;
}

//* Description: the picture buf is secure
static int __LayerSetSecure(LayerCtrl* l, int bSecure)
{
    logv("__LayerSetSecure, bSecure = %d", bSecure);
    LayerContext* lc;

    lc = (LayerContext*)l;

    lc->bProtectFlag = bSecure;

    return 0;
}

static int __LayerReleaseBuffer(LayerCtrl* l, VideoPicture* pPicture)
{
    logd("LayerReleaseBuffer,pPicture = %p",pPicture);
    LayerContext* lc;

    lc = (LayerContext*)l;
    if(lc->pMemops)
        SunxiMemPfree(lc->pMemops, pPicture->pData0);
    return 0;
}

static int __LayerControl(LayerCtrl* l, int cmd, void *para)
{
    LayerContext* lc;

    lc = (LayerContext*)l;

    logd("layer control cmd = %d",cmd);
    if(CDX_LAYER_CMD_SET_HDR_INFO == cmd){
        logd("get the fbm buf info");
        FbmBufInfo* fbmBufInfo = (FbmBufInfo*)para;
        logd("fbmBufInfo->bProgressiveFlag = %d",fbmBufInfo->bProgressiveFlag);
        if(fbmBufInfo->bProgressiveFlag){
            lc->mNumHoldByLayer = GetConfigParamterInt("pic_4list_num", 3);
        }else{
            lc->mNumHoldByLayer = GetConfigParamterInt("pic_4list_num", 3) + GetConfigParamterInt("pic_4di_num", 2);
        }
        logd("lc->mNumHoldByLayer = %d",lc->mNumHoldByLayer);
    }
    if(CDX_LAYER_CMD_SET_VIDEOCODEC_LBC_MODE == cmd){
        logd("get the fbm buf info");
        FbmBufInfo* fbmBufInfo = (FbmBufInfo*)para;
        lc->b10BitPicFlag = fbmBufInfo->b10bitVideoFlag;
        lc->nLbcLossyComMod = fbmBufInfo->nLbcLossyComMod;
        lc->bIsLossy        = fbmBufInfo->bIsLossy;
        lc->bRcEn           = fbmBufInfo->bRcEn;
        logd("b10BitPicFlag = %d, nLbcLossyComMod = %d, bIsLossy = %u, bRcEn = %u", lc->b10BitPicFlag, lc->nLbcLossyComMod, lc->bIsLossy, lc->bRcEn);
    }
    return 0;
}


static LayerControlOpsT mLayerControlOps =
{
    release:                    __LayerRelease                   ,

    setSecureFlag:              __LayerSetSecure                 ,
    setDisplayBufferSize:       __LayerSetDisplayBufferSize      ,
    setDisplayBufferCount:      __LayerSetDisplayBufferCount     ,
    setDisplayRegion:           __LayerSetDisplayRegion          ,
    setDisplayPixelFormat:      __LayerSetDisplayPixelFormat     ,
    setVideoWithTwoStreamFlag:  __LayerSetVideoWithTwoStreamFlag ,
    setIsSoftDecoderFlag:       __LayerSetIsSoftDecoderFlag      ,
    setBufferTimeStamp:         __LayerSetBufferTimeStamp        ,

    resetNativeWindow :         __LayerResetNativeWindow         ,
    getBufferOwnedByGpu:        __LayerGetBufferOwnedByGpu       ,
    getDisplayFPS:              __LayerGetDisplayFPS             ,
    getBufferNumHoldByGpu:      __LayerGetBufferNumHoldByGpu     ,

    ctrlShowVideo :             __LayerCtrlShowVideo             ,
    ctrlHideVideo:              __LayerCtrlHideVideo             ,
    ctrlIsVideoShow:            __LayerCtrlIsVideoShow           ,
    ctrlHoldLastPicture :       __LayerCtrlHoldLastPicture       ,

    dequeueBuffer:              __LayerDequeueBuffer             ,
    queueBuffer:                __LayerQueueBuffer               ,
    releaseBuffer:              __LayerReleaseBuffer             ,
    reset:                      __LayerReset                     ,
    destroy:                    __LayerDestroy,
    control:                    __LayerControl,
};

LayerCtrl* LayerCreate(VideoFrameCallback callback,void* pUser)
{
    LayerContext* lc;
    unsigned int args[4];
    int screen_id;
    int enable = 0;
	int rotate = 0;
	VoutRect rect;
    logd("LayerCreate.");
    int init_ret = -1;
    if(init_mutex_count == 0){
        init_ret = pthread_mutex_init(&configMtx, NULL);
        logd("init_ret = %d",init_ret);
    }
    init_mutex_count++;
    logd("init_mutex_count = %d",init_mutex_count);
    lc = (LayerContext*)malloc(sizeof(LayerContext));
    if(lc == NULL)
    {
        loge("malloc memory fail.");
        return NULL;
    }
    memset(lc, 0, sizeof(LayerContext));
	lc->mDispOutPort = CreateVideoOutport(0);
	rect.x = 0;
	rect.y = 0;
	rect.width = 320;
	rect.height = 240;
	pthread_mutex_lock(&configMtx);
	lc->mDispOutPort->init(lc->mDispOutPort, enable, rotate, &rect);
	pthread_mutex_unlock(&configMtx);
    lc->base.ops = &mLayerControlOps;
    lc->eDisplayPixelFormat = PIXEL_FORMAT_NV21;

    lc->mVideoframeCallback = callback;
    lc->pUserData = pUser;
    logd("==== callback: %p, pUser: %p", callback, pUser);
    /* get screen size. */
	pthread_mutex_lock(&configMtx);
    lc->nScreenWidth = lc->mDispOutPort->getScreenWidth(lc->mDispOutPort);
	lc->nScreenHeight = lc->mDispOutPort->getScreenHeight(lc->mDispOutPort);
	pthread_mutex_unlock(&configMtx);
	rect.x = 0;
	rect.y = 0;
	rect.width = lc->nScreenWidth;
	rect.height = lc->nScreenHeight;
	lc->dst_rect.x = 0;
	lc->dst_rect.y = 0;
	lc->dst_rect.width = lc->nScreenWidth;
	lc->dst_rect.height = lc->nScreenHeight;
	lc->setDstRectFlag    = 1;
	pthread_mutex_lock(&configMtx);
	lc->mDispOutPort->setRectFake(lc->mDispOutPort,&rect);
	pthread_mutex_unlock(&configMtx);
    logd("screen:w %d, screen:h %d", lc->nScreenWidth, lc->nScreenHeight);
    lc->pMemops = GetMemAdapterOpsS();
    int openRet = SunxiMemOpen(lc->pMemops);
    if(openRet == -1){
        loge("SunxiMemOpen fail");
        lc->pMemops = NULL;
    }
    return &lc->base;
}

void LayerSetControl(LayerCtrl* l, LAYER_CMD_TYPE cmd, int grade)
{
    LayerContext* lc;
    int i;

    lc = (LayerContext*)l;
	switch(cmd){
		case VIDEO_CMD_SET_BRIGHTNESS:
		case VIDEO_CMD_SET_CONTRAST:
		case VIDEO_CMD_SET_HUE:
		case VIDEO_CMD_SET_SATURATION:
		case VIDEO_CMD_SET_VEDIO_ENHANCE_DEFAULT:
			pthread_mutex_lock(&configMtx);
			lc->mDispOutPort->setIoctl(lc->mDispOutPort, cmd, grade);
			pthread_mutex_unlock(&configMtx);
			break;
		default:
			break;
	}
}

int LayerSetSrcRect(LayerCtrl* l, int x, int y, unsigned int width, unsigned int height)
{
	LayerContext* lc;

    lc = (LayerContext*)l;
    logd("Layer set src rect,(%d %d, %dx%d)\n",
            x, y, width, height);

    if(width == 0 && height == 0)
        return -1;

	lc->src_rect.x = x;
	lc->src_rect.y = y;
	lc->src_rect.width = width;
	lc->src_rect.height = height;
	lc->setSrcRectFlag    = 1;
	pthread_mutex_lock(&configMtx);
	lc->mDispOutPort->setSrcRect(lc->mDispOutPort, &lc->src_rect);
	pthread_mutex_unlock(&configMtx);
    return 0;
}
int LayerSetDisplayRect(LayerCtrl* l, int x, int y, unsigned int width, unsigned int height)
{
	LayerContext* lc;
    lc = (LayerContext*)l;
    logd("Layer set display rect,(%d %d, %dx%d)\n",
            x, y, width, height);
    if(width == 0 && height == 0)
        return -1;
	lc->dst_rect.x = x;
	lc->dst_rect.y = y;
	lc->dst_rect.width = width;
	lc->dst_rect.height = height;
	lc->setDstRectFlag    = 1;
	pthread_mutex_lock(&configMtx);
	lc->mDispOutPort->setRectFake(lc->mDispOutPort, &lc->dst_rect);
	pthread_mutex_unlock(&configMtx);
    return 0;
}

int LayerGetDisplayRect(LayerCtrl* l, VoutRect *pRect)
{
	LayerContext* lc;
    lc = (LayerContext*)l;
	memcpy(pRect,&lc->dst_rect,sizeof(VoutRect));
	return 0;
}
int LayerDisplayOnoff(LayerCtrl* l, int onoff)
{
	LayerContext* lc;

    lc = (LayerContext*)l;
	if(onoff)
		lc->bLayerShowed = 1;
	else
		lc->bLayerShowed = 0;

	return 0;
}

int LayerSetG2dRotateDegree(LayerCtrl* l, int degree)
{
	LayerContext* lc;

    lc = (LayerContext*)l;
    logd("Layer set g2d rotate degree %d.", degree);

    if(degree < 0)
        return -1;

	lc->mG2dRotateDegree = degree;
    return 0;
}

int createRotateBuf(LayerCtrl* l,int width,int height){
    int ret = 0;
    LayerContext* lc;
    lc = (LayerContext*)l;
    int i = 0;
    char* pVirBuf;
    char* pPhyBuf;
    int nBufferFd = 0;
    logd("before align:width = %d,height = %d",width,height);
    width = ALIGN_32B(width);
    height = ALIGN_32B(height);
    logd("after align:width = %d,height = %d",width,height);
    for(i=0; i<ROTATE_BUFFER_NUM; i++)
    {
        pVirBuf = (char*)SunxiMemPalloc(lc->pMemops, width * height * 3/2);
        if(pVirBuf == NULL){
            loge("SunxiMemPalloc err");
            ret = -1;
            break;
        }
#ifdef CONF_KERNEL_IOMMU
        nBufferFd = SunxiMemGetBufferFd(lc->pMemops,pVirBuf);
#else
        pPhyBuf = (char*)SunxiMemGetPhysicAddressCpu(lc->pMemops, pVirBuf);
#endif
        lc->mRotateBufInfo[i].nUsedFlag    = 0;

        lc->mRotateBufInfo[i].pPicture.nWidth  = width;
        lc->mRotateBufInfo[i].pPicture.nHeight = height;
        lc->mRotateBufInfo[i].pPicture.nLineStride  = width;
        lc->mRotateBufInfo[i].pPicture.pData0       = pVirBuf;
        lc->mRotateBufInfo[i].pPicture.pData1       = pVirBuf + (width * height);
        lc->mRotateBufInfo[i].pPicture.pData2       = lc->mRotateBufInfo[i].pPicture.pData1 + (width * height)/4;
#ifdef CONF_KERNEL_IOMMU
        lc->mRotateBufInfo[i].pPicture.nBufFd   = nBufferFd;
#else
        lc->mRotateBufInfo[i].pPicture.phyYBufAddr  = (uintptr_t)pPhyBuf;
        lc->mRotateBufInfo[i].pPicture.phyCBufAddr  =
        lc->mRotateBufInfo[i].pPicture.phyYBufAddr + (width * height);
#endif
        lc->mRotateBufInfo[i].pPicture.nBufId       = i;
        lc->mRotateBufInfo[i].pPicture.ePixelFormat = lc->eDisplayPixelFormat;
    }
    /*if has err,we should free the palloced buf*/
    if(ret == -1){
         int j = 0;
         for(j = 0;j < i;j++){
            SunxiMemPfree(lc->pMemops, lc->mRotateBufInfo[j].pPicture.pData0);
            lc->mRotateBufInfo[j].pPicture.pData0 = NULL;
         }
    }
    return ret;
}
int hwRotateVideoPicture(LayerCtrl* l,VideoPicture* pPictureIn,VideoPicture* pPictureOut,int  nRotateDegree){
    int ret = 0;
    LayerContext* lc;
    lc = (LayerContext*)l;
    unsigned long arg[4] = {0};
    tr_info tTrInfo;
    int inCroppedLeft = pPictureIn->nLeftOffset;
    int inCroppedRight = pPictureIn->nWidth - pPictureIn->nRightOffset;
    int inCroppedTop = pPictureIn->nTopOffset;
    int inCroppedBottom = pPictureIn->nHeight - pPictureIn->nBottomOffset;
    int outCroppedLeft = 0;
    int outCroppedRitht = 0;
    int outCroppedTop = 0;
    int outCroppedBottom = 0;
setup_tr:

    if(lc->mInitedTransformFlag == 0)
    {
        //* open the tr driver
        lc->mTransformFd= open("/dev/transform",O_RDWR);
        logv("open transform drive ret = %d",lc->mTransformFd);

        if(lc->mTransformFd < 0)
        {
            loge("*****open tr driver fail!");
            return -1;
        }

        //* request tr channel
        arg[0] = (unsigned long)&(lc->mTransformChannel);
        if(ioctl(lc->mTransformFd,TR_REQUEST,(void*)arg)<0){
            loge("#######error: tr_request failed!");
            return -1;
        }
        logd("request tr channel  = 0x%x",lc->mTransformChannel);

        //* set tr timeout
        arg[0] = lc->mTransformChannel;
        arg[1] = TRANSFORM_DEV_TIMEOUT;
        if(ioctl(lc->mTransformFd,TR_SET_TIMEOUT,(void*)arg) != 0)
        {
            loge("#######error: tr_set_timeout failed!");
            return -1;
        }

        lc->mInitedTransformFlag = 1;
    }

    if(lc->mTransformFd < 0)
    {
        loge("the lc->mTransformFd is invalid : %d",lc->mTransformFd);
        return -1;
    }

    pPictureOut->nPts = pPictureIn->nPts;
    //* set rotation angle and the cropped of output picture
    if(nRotateDegree == 0){
        tTrInfo.mode = TR_ROT_0;
        outCroppedLeft = inCroppedLeft;
        outCroppedRitht = inCroppedRight;
        outCroppedTop = inCroppedTop;
        outCroppedBottom = inCroppedBottom;
    }else if(nRotateDegree == 90){
        tTrInfo.mode = TR_ROT_90;
        outCroppedLeft = inCroppedBottom;
        outCroppedRitht = inCroppedTop;
        outCroppedTop = inCroppedLeft;
        outCroppedBottom = inCroppedRight;
    }else if(nRotateDegree == 180){
        tTrInfo.mode = TR_ROT_180;
        outCroppedLeft = inCroppedRight;
        outCroppedRitht = inCroppedLeft;
        outCroppedTop = inCroppedBottom;
        outCroppedBottom = inCroppedTop;
    }else if(nRotateDegree == 270){
        tTrInfo.mode = TR_ROT_270;
        outCroppedLeft = inCroppedTop;
        outCroppedRitht = inCroppedBottom;
        outCroppedTop = inCroppedRight;
        outCroppedBottom = inCroppedLeft;
    }else{
        tTrInfo.mode = TR_ROT_0;
        outCroppedLeft = inCroppedLeft;
        outCroppedRitht = inCroppedRight;
        outCroppedTop = inCroppedTop;
        outCroppedBottom = inCroppedBottom;
    }
    pPictureOut->nLeftOffset = outCroppedLeft;
    pPictureOut->nRightOffset = pPictureOut->nWidth - outCroppedRitht;
    pPictureOut->nTopOffset = outCroppedTop;
    pPictureOut->nBottomOffset = pPictureOut->nHeight - outCroppedBottom;
    /*
    logd("outCroppedLeft = %d,outCroppedRitht = %d,outCroppedTop = %d,outCroppedBottom = %d",
        outCroppedLeft,outCroppedRitht,outCroppedTop,outCroppedBottom);
    logd("pPictureOut->nLeftOffset = %d,nRightOffset = %d,nTopOffset = %d,nBottomOffset = %d",
        pPictureOut->nLeftOffset,pPictureOut->nRightOffset,pPictureOut->nTopOffset,pPictureOut->nBottomOffset);
    */

    //* set in picture
    if(pPictureIn->ePixelFormat == PIXEL_FORMAT_YV12){
        tTrInfo.src_frame.fmt = TR_FORMAT_YUV420_P;
    }
    else if(pPictureIn->ePixelFormat == PIXEL_FORMAT_NV21){
        tTrInfo.src_frame.fmt = TR_FORMAT_YUV420_SP_UVUV;
    }
    else
    {
        loge("***the ePixelFormat[0x%x] is not support by tr driver",pPictureIn->ePixelFormat);
        return -1;
    }

    tTrInfo.src_frame.laddr[0]  = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureIn->pData0);
    tTrInfo.src_frame.laddr[1]  = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureIn->pData1);
    tTrInfo.src_frame.laddr[2]  = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops, (void*)pPictureIn->pData2);

    tTrInfo.src_frame.pitch[0]  = ALIGN_32B(pPictureIn->nLineStride);
    tTrInfo.src_frame.pitch[1]  = ALIGN_32B(pPictureIn->nLineStride)/2;
    tTrInfo.src_frame.pitch[2]  = ALIGN_32B(pPictureIn->nLineStride)/2;

    tTrInfo.src_frame.height[0] = ALIGN_32B(pPictureIn->nHeight);
    tTrInfo.src_frame.height[1] = ALIGN_32B(pPictureIn->nHeight)/2;
    tTrInfo.src_frame.height[2] = ALIGN_32B(pPictureIn->nHeight)/2;

    tTrInfo.src_rect.x = 0;
    tTrInfo.src_rect.y = 0;
    tTrInfo.src_rect.w = pPictureIn->nWidth;
    tTrInfo.src_rect.h = pPictureIn->nHeight;

    /*because the transform only support yv12,so we should set the format of rotated picture yv12*/
    pPictureOut->ePixelFormat = PIXEL_FORMAT_YV12;
    /*the output format only support yuv420P*/
    tTrInfo.dst_frame.fmt = TR_FORMAT_YUV420_P;
    tTrInfo.dst_frame.laddr[0]  = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops, (void*)pPictureOut->pData0);
    tTrInfo.dst_frame.laddr[1]  = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureOut->pData1);
    tTrInfo.dst_frame.laddr[2]  = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops, (void*)pPictureOut->pData2);

    tTrInfo.dst_frame.pitch[0]  = ALIGN_32B(pPictureOut->nLineStride);
    tTrInfo.dst_frame.pitch[1]  = ALIGN_32B(pPictureOut->nLineStride)/2;
    tTrInfo.dst_frame.pitch[2]  = ALIGN_32B(pPictureOut->nLineStride)/2;

    tTrInfo.dst_frame.height[0] = ALIGN_32B(pPictureOut->nHeight);
    tTrInfo.dst_frame.height[1] = ALIGN_32B(pPictureOut->nHeight)/2;
    tTrInfo.dst_frame.height[2] = ALIGN_32B(pPictureOut->nHeight)/2;

    tTrInfo.dst_rect.x = 0;
    tTrInfo.dst_rect.y = 0;
    tTrInfo.dst_rect.w = pPictureOut->nWidth;
    tTrInfo.dst_rect.h = pPictureOut->nHeight;


    //* setup rotate
    arg[0] = lc->mTransformChannel;
    arg[1] = (size_addr)&tTrInfo;
    arg[2] = 0;
    arg[3] = 0;

    logv("***setup rotate start!: arg: 0x%x, 0x%x",arg[0],arg[1]);
    if(ioctl(lc->mTransformFd,TR_COMMIT,(void*)arg) != 0)
    {
        loge("######error: setup rotate failed!");
        return -1;
    }
    logv("***setup rotate finish!");

    //* wait
    arg[0] = lc->mTransformChannel;
    arg[1] = 0;
    arg[2] = 0;
    arg[3] = 0;

    logv("***tr query start!");
    int nTimeOut = 0;
    ret = ioctl(lc->mTransformFd,TR_QUERY,(void*)arg);

    while(ret == 1)// 0 : suceef; 1:busy ; -1:timeOut
    {
        logv("***tr sleep!");
        usleep(1*1000);
        ret = ioctl(lc->mTransformFd,TR_QUERY,(void*)arg);
    }

    //* if the tr is timeout ,we should setup tr again
    if(ret == -1)
        goto setup_tr;

    logv("***tr query finish!");
    return ret;
}
#if G2D_ENBLE
int createG2dRotateBuf(LayerCtrl* l,VideoPicture* pBuf,int rotateDegree){
    int ret = 0;
    LayerContext* lc;
    lc = (LayerContext*)l;
    int i = 0;
    char* pVirBuf;
    char* pPhyBuf;
    int nBufferFd = 0;
	int width = 0;
	int height = 0;
    int nG2dBufferSize;
	if(rotateDegree == 90 || rotateDegree == 270)
	{
		width = pBuf->nHeight;
		height = pBuf->nWidth;
	}
	else
	{
		width = pBuf->nWidth;
		height = pBuf->nHeight;
	}
    logd("width = %d,height = %d",width,height);
    nG2dBufferSize = pBuf->nLbcSize + (128 * 1024);
    for(i=0; i<ROTATE_BUFFER_NUM; i++)
    {
    	if(pBuf->nLbcLossyComMod == 0)
    	{
    		pVirBuf = (char*)SunxiMemPalloc(lc->pMemops, width * height * 3/2);
    	}
		else
		{
			pVirBuf = (char*)SunxiMemPalloc(lc->pMemops, nG2dBufferSize);
		}
        if(pVirBuf == NULL){
            loge("SunxiMemPalloc err");
            ret = -1;
            break;
        }
#ifdef CONF_KERNEL_IOMMU
        nBufferFd = SunxiMemGetBufferFd(lc->pMemops,pVirBuf);
#else
        pPhyBuf = (char*)SunxiMemGetPhysicAddressCpu(lc->pMemops, pVirBuf);
#endif
        lc->mRotateBufInfo[i].nUsedFlag    = 0;
        lc->mRotateBufInfo[i].pPicture.nWidth  = width;
        lc->mRotateBufInfo[i].pPicture.nHeight = height;
        lc->mRotateBufInfo[i].pPicture.nLineStride  = width;
		if(pBuf->nLbcLossyComMod == 0)
		{
		lc->mRotateBufInfo[i].pPicture.pData0       = pVirBuf;
		lc->mRotateBufInfo[i].pPicture.pData1       = pVirBuf + (width * height);
		lc->mRotateBufInfo[i].pPicture.pData2       = lc->mRotateBufInfo[i].pPicture.pData1 + (width * height)/4;
		}
		else
		{
			lc->mRotateBufInfo[i].pPicture.pData0       = pVirBuf;
			lc->mRotateBufInfo[i].pPicture.pData1 = NULL;
			lc->mRotateBufInfo[i].pPicture.pData2 = NULL;
		}
#ifdef CONF_KERNEL_IOMMU
        lc->mRotateBufInfo[i].pPicture.nBufFd   = nBufferFd;
#else
        lc->mRotateBufInfo[i].pPicture.phyYBufAddr  = (uintptr_t)pPhyBuf;
		if(pBuf->nLbcLossyComMod == 0)
		{
		    lc->mRotateBufInfo[i].pPicture.phyCBufAddr  =
			lc->mRotateBufInfo[i].pPicture.phyYBufAddr + (width * height);
		}
		else
		{
			lc->mRotateBufInfo[i].pPicture.phyCBufAddr = 0;
		}
#endif
		lc->mRotateBufInfo[i].pPicture.nLbcLossyComMod = pBuf->nLbcLossyComMod;
		lc->mRotateBufInfo[i].pPicture.bIsLossy = pBuf->bIsLossy;
		lc->mRotateBufInfo[i].pPicture.bRcEn = pBuf->bRcEn;
		lc->mRotateBufInfo[i].pPicture.nLbcSize = nG2dBufferSize;
        lc->mRotateBufInfo[i].pPicture.nBufId = i;
        lc->mRotateBufInfo[i].pPicture.ePixelFormat = pBuf->ePixelFormat;
    }
    /*if has err,we should free the palloced buf*/
    if(ret == -1){
         int j = 0;
         for(j = 0;j < i;j++){
            SunxiMemPfree(lc->pMemops, lc->mRotateBufInfo[j].pPicture.pData0);
            lc->mRotateBufInfo[j].pPicture.pData0 = NULL;
         }
        return ret;
    }

    //Since,open the lbc,G2D does not support direct rotation of 180��,
    //the buffer is used to assist the rotation of 90�� twice.
    if ((rotateDegree == 180) && (pBuf->nLbcLossyComMod != 0))
    {
		width = pBuf->nHeight;
		height = pBuf->nWidth;

        pVirBuf = (char*)SunxiMemPalloc(lc->pMemops, nG2dBufferSize);
        if(pVirBuf == NULL){
            loge("SunxiMemPalloc err");
            return -1;
        }

#ifdef CONF_KERNEL_IOMMU
        nBufferFd = SunxiMemGetBufferFd(lc->pMemops,pVirBuf);
#else
        pPhyBuf = (char*)SunxiMemGetPhysicAddressCpu(lc->pMemops, pVirBuf);
#endif
        lc->mRotateExtraBufInfo.nUsedFlag    = 0;
        lc->mRotateExtraBufInfo.pPicture.nWidth  = width;
        lc->mRotateExtraBufInfo.pPicture.nHeight = height;
        lc->mRotateExtraBufInfo.pPicture.nLineStride  = width;
        lc->mRotateExtraBufInfo.pPicture.pData0 = pVirBuf;
        lc->mRotateExtraBufInfo.pPicture.pData1 = NULL;
        lc->mRotateExtraBufInfo.pPicture.pData2 = NULL;

#ifdef CONF_KERNEL_IOMMU
        lc->mRotateExtraBufInfo.pPicture.nBufFd   = nBufferFd;
#else
        lc->mRotateExtraBufInfo.pPicture.phyYBufAddr  = (uintptr_t)pPhyBuf;
        lc->mRotateExtraBufInfo.pPicture.phyCBufAddr = 0;
#endif
        lc->mRotateExtraBufInfo.pPicture.nLbcLossyComMod = pBuf->nLbcLossyComMod;
        lc->mRotateExtraBufInfo.pPicture.bIsLossy = pBuf->bIsLossy;
        lc->mRotateExtraBufInfo.pPicture.bRcEn = pBuf->bRcEn;
        lc->mRotateExtraBufInfo.pPicture.nLbcSize = nG2dBufferSize;
        lc->mRotateExtraBufInfo.pPicture.nBufId = i+1;
        lc->mRotateExtraBufInfo.pPicture.ePixelFormat = pBuf->ePixelFormat;
    }

    return 0;
}
int hwG2dRotateVideoPicture(LayerCtrl* l,VideoPicture* pPictureIn,VideoPicture* pPictureOut,int  nRotateDegree){
    int ret = 0;
    LayerContext* lc;
    lc = (LayerContext*)l;
    unsigned long arg[4] = {0};
    int inCroppedLeft = pPictureIn->nLeftOffset;
    int inCroppedRight =  pPictureIn->nRightOffset;
    int inCroppedTop = pPictureIn->nTopOffset;
    int inCroppedBottom = pPictureIn->nBottomOffset;
    int outCroppedLeft = 0;
    int outCroppedRitht = 0;
    int outCroppedTop = 0;
    int outCroppedBottom = 0;
    g2d_blt_h   blit;
	g2d_lbc_rot info;

    if(lc->mInitedTransformFlag == 0)
    {
        //* open the tr driver
        lc->mTransformFd= open("/dev/g2d", O_RDWR);
        logd("open transform drive ret = %d",lc->mTransformFd);
        if(lc->mTransformFd < 0)
        {
            loge("*****open tr driver fail!");
            return -1;
        }
		logd("pPictureIn:%d,%d,%d,%d\n",pPictureIn->nLeftOffset,pPictureIn->nRightOffset, \
				pPictureIn->nTopOffset,pPictureIn->nBottomOffset);
		logd("[in]w:%d,h:%d,nLbcLossyComMod:%d\n",pPictureIn->nWidth,pPictureIn->nHeight,pPictureIn->nLbcLossyComMod);
		logd("bRcEn:%d,bIsLossy:%d\n",pPictureIn->bRcEn,pPictureIn->bIsLossy);
        lc->mInitedTransformFlag = 1;
    }
    if(lc->mTransformFd < 0)
    {
        loge("the lc->mTransformFd is invalid : %d",lc->mTransformFd);
        return -1;
    }
    pPictureOut->nPts = pPictureIn->nPts;

    //* set rotation angle and the cropped of output picture
    if(nRotateDegree == 0){
        outCroppedLeft = inCroppedLeft;
        outCroppedRitht = inCroppedRight;
        outCroppedTop = inCroppedTop;
        outCroppedBottom = inCroppedBottom;
    }else if(nRotateDegree == 90){
        outCroppedLeft = inCroppedTop;
        outCroppedRitht = inCroppedBottom;
        outCroppedTop = inCroppedLeft;
        outCroppedBottom = inCroppedRight;
    }else if(nRotateDegree == 180){
        outCroppedLeft = inCroppedLeft;
        outCroppedRitht = inCroppedRight;
        outCroppedTop = inCroppedTop;
        outCroppedBottom = inCroppedBottom;

    }else if(nRotateDegree == 270){
        outCroppedLeft = inCroppedTop;
        outCroppedRitht = inCroppedBottom;
        outCroppedTop = inCroppedLeft;
        outCroppedBottom = inCroppedRight;

    }else{
        outCroppedLeft = inCroppedLeft;
        outCroppedRitht = inCroppedRight;
        outCroppedTop = inCroppedTop;
        outCroppedBottom = inCroppedBottom;
    }
	pPictureOut->nLeftOffset = outCroppedLeft;
    pPictureOut->nRightOffset = outCroppedRitht;
    pPictureOut->nTopOffset = outCroppedTop;
    pPictureOut->nBottomOffset = outCroppedBottom;

    /*
    logd("outCroppedLeft = %d,outCroppedRitht = %d,outCroppedTop = %d,outCroppedBottom = %d",
        outCroppedLeft,outCroppedRitht,outCroppedTop,outCroppedBottom);
    logd("pPictureOut->nLeftOffset = %d,nRightOffset = %d,nTopOffset = %d,nBottomOffset = %d",
        pPictureOut->nLeftOffset,pPictureOut->nRightOffset,pPictureOut->nTopOffset,pPictureOut->nBottomOffset);
    */
#if 1
	if(pPictureIn->nLbcLossyComMod == 0)
	{
	    memset(&blit, 0, sizeof(g2d_blt_h));
	    //* set in picture
	    if(pPictureIn->ePixelFormat == PIXEL_FORMAT_YV12)
		{
			blit.src_image_h.format = G2D_FORMAT_YUV420_PLANAR;//g2d_fmt_enh
			blit.dst_image_h.format = G2D_FORMAT_YUV420_PLANAR;
			blit.src_image_h.laddr[0] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureIn->pData0);
		    blit.src_image_h.laddr[1] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureIn->pData1);
		    blit.src_image_h.laddr[2] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureIn->pData2);
			blit.dst_image_h.laddr[0] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureOut->pData0);
		    blit.dst_image_h.laddr[1] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureOut->pData1);
			blit.dst_image_h.laddr[2] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureOut->pData2);
	    }
	    else if(pPictureIn->ePixelFormat == PIXEL_FORMAT_NV21)
		{
			blit.src_image_h.format = G2D_FORMAT_YUV420UVC_U1V1U0V0;//g2d_fmt_enh
			blit.dst_image_h.format = G2D_FORMAT_YUV420UVC_U1V1U0V0;
			blit.src_image_h.laddr[0] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureIn->pData0);
		    blit.src_image_h.laddr[1] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureIn->pData1);
		    blit.src_image_h.laddr[2] = 0;
			blit.dst_image_h.laddr[0] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureOut->pData0);
		    blit.dst_image_h.laddr[1] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureOut->pData1);
		    blit.dst_image_h.laddr[2] = 0;
	    }
	    else
	    {
	        loge("***the ePixelFormat[0x%x] is not support by tr driver",pPictureIn->ePixelFormat);
	        return -1;
	    }
	    switch(nRotateDegree)
	    {
	        case 0:
	            blit.flag_h = G2D_ROT_0;
	            break;
	        case 90:
	            blit.flag_h = G2D_ROT_90;
	            break;
	        case 180:
	            blit.flag_h = G2D_ROT_180;
	            break;
	        case 270:
	            blit.flag_h = G2D_ROT_270;
	            break;
	        case 1000:
	            blit.flag_h = G2D_ROT_H;
	            break;
	        case 2000:
	            blit.flag_h = G2D_ROT_V;
	            break;
	        default:
	            logw("fatal error! rot_angle[%d] is invalid!\n", nRotateDegree);
	            blit.flag_h = G2D_BLT_NONE_H;
	            break;
	    }
	    blit.src_image_h.bbuff = 1;
	    blit.src_image_h.use_phy_addr = 1;
	    blit.src_image_h.color = 0xff;
	    blit.src_image_h.width = pPictureIn->nWidth;
	    blit.src_image_h.height = pPictureIn->nHeight;
	    blit.src_image_h.align[0] = 0;
	    blit.src_image_h.align[1] = 0;
	    blit.src_image_h.align[2] = 0;
	    blit.src_image_h.clip_rect.x = 0;
	    blit.src_image_h.clip_rect.y = 0;
	    blit.src_image_h.clip_rect.w = pPictureIn->nRightOffset - pPictureIn->nLeftOffset;
	    blit.src_image_h.clip_rect.h = pPictureIn->nBottomOffset - pPictureIn->nTopOffset;
	    blit.src_image_h.gamut = G2D_BT709;
	    blit.src_image_h.bpremul = 0;
	    blit.src_image_h.alpha = 0xff;
	    blit.src_image_h.mode = G2D_GLOBAL_ALPHA;
	    blit.dst_image_h.bbuff = 1;
	    blit.dst_image_h.use_phy_addr = 1;
	    blit.dst_image_h.color = 0xff;
	    blit.dst_image_h.width = pPictureOut->nWidth;
	    blit.dst_image_h.height = pPictureOut->nHeight;
	    blit.dst_image_h.align[0] = 0;
	    blit.dst_image_h.align[1] = 0;
	    blit.dst_image_h.align[2] = 0;
	    blit.dst_image_h.clip_rect.x = 0;
	    blit.dst_image_h.clip_rect.y = 0;
	    blit.dst_image_h.clip_rect.w = pPictureOut->nRightOffset - pPictureOut->nLeftOffset;
	    blit.dst_image_h.clip_rect.h = pPictureOut->nBottomOffset - pPictureOut->nTopOffset;
	    blit.dst_image_h.gamut = G2D_BT709;
	    blit.dst_image_h.bpremul = 0;
	    blit.dst_image_h.alpha = 0xff;
	    blit.dst_image_h.mode = G2D_GLOBAL_ALPHA;
	    ret = ioctl(lc->mTransformFd,G2D_CMD_BITBLT_H,(unsigned long)&blit);
	    if (ret<0)
	    {
	        loge("g2d G2D_CMD_BITBLT_H fail\n");
	        ret = -1;
		}
	    else
	    {
	    }
	}
	else
	{
        memset(&info, 0, sizeof(g2d_lbc_rot));
        switch(nRotateDegree)
        {
            case 0:
                info.blt.flag_h = G2D_ROT_0;
                break;
            case 90:
                info.blt.flag_h = G2D_ROT_90;
                break;
            case 180:
                info.blt.flag_h = G2D_ROT_180;
                break;
            case 270:
                info.blt.flag_h = G2D_ROT_270;
                break;
            case 1000:
                info.blt.flag_h = G2D_ROT_H;
                break;
             case 2000:
                info.blt.flag_h = G2D_ROT_V;
                break;
            default:
                loge("fatal error! rot_angle[%d] is invalid!\n", nRotateDegree);
                info.blt.flag_h = G2D_BLT_NONE_H;
                break;
        }
        ////////////////////////////////////////////////////////////
        switch(pPictureIn->nLbcLossyComMod)
        {
            case 1:
                info.lbc_cmp_ratio = 666;
                break;
            case 2:
                info.lbc_cmp_ratio = 500;
                break;
            case 3:
                info.lbc_cmp_ratio = 400;
                break;
            default:
                logw("nLbcMod:%d not support!\n",pPictureIn->nLbcLossyComMod);
                break;
        }

        //Since,open the lbc,G2D does not support direct rotation of 180��,
        //the buffer is used to assist the rotation of 90�� twice.
        if (nRotateDegree == 180)
        {
            info.blt.flag_h = G2D_ROT_90;

            //first rotate 0��->90��
            info.enc_is_lossy = pPictureIn->bIsLossy;
            info.dec_is_lossy = pPictureIn->bIsLossy;
            info.blt.src_image_h.bbuff = 1;
            info.blt.src_image_h.use_phy_addr = 1;
            info.blt.src_image_h.color = 0xff;
            info.blt.src_image_h.format = G2D_FORMAT_YUV420_PLANAR;//g2d_fmt_enh
            info.blt.src_image_h.laddr[0] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureIn->pData0);
            info.blt.src_image_h.laddr[1] = 0;
            info.blt.src_image_h.laddr[2] = 0;
            info.blt.src_image_h.width = pPictureIn->nWidth;
            info.blt.src_image_h.height = pPictureIn->nHeight;
            info.blt.src_image_h.align[0] = 0;
            info.blt.src_image_h.align[1] = 0;
            info.blt.src_image_h.align[2] = 0;
            info.blt.src_image_h.clip_rect.x = 0;
            info.blt.src_image_h.clip_rect.y = 0;
            info.blt.src_image_h.clip_rect.w = pPictureIn->nWidth;
            info.blt.src_image_h.clip_rect.h = pPictureIn->nHeight;
            info.blt.src_image_h.gamut = G2D_BT709;
            info.blt.src_image_h.bpremul = 0;
            info.blt.src_image_h.alpha = 0xff;
            info.blt.src_image_h.mode = G2D_GLOBAL_ALPHA;
            info.blt.dst_image_h.bbuff = 1;
            info.blt.dst_image_h.use_phy_addr = 1;
            info.blt.dst_image_h.color = 0xff;
            info.blt.dst_image_h.format = G2D_FORMAT_YUV420_PLANAR;
            info.blt.dst_image_h.laddr[0] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops, (void *)lc->mRotateExtraBufInfo.pPicture.pData0);
            info.blt.dst_image_h.laddr[1] = 0;
            info.blt.dst_image_h.laddr[2] = 0;
            info.blt.dst_image_h.width = lc->mRotateExtraBufInfo.pPicture.nWidth;
            info.blt.dst_image_h.height = lc->mRotateExtraBufInfo.pPicture.nHeight;
            info.blt.dst_image_h.align[0] = 0;
            info.blt.dst_image_h.align[1] = 0;
            info.blt.dst_image_h.align[2] = 0;
            info.blt.dst_image_h.clip_rect.x = 0;
            info.blt.dst_image_h.clip_rect.y = 0;
            info.blt.dst_image_h.clip_rect.w = lc->mRotateExtraBufInfo.pPicture.nWidth;
            info.blt.dst_image_h.clip_rect.h = lc->mRotateExtraBufInfo.pPicture.nHeight;
            info.blt.dst_image_h.gamut = G2D_BT709;
            info.blt.dst_image_h.bpremul = 0;
            info.blt.dst_image_h.alpha = 0xff;
            info.blt.dst_image_h.mode = G2D_GLOBAL_ALPHA;
            ret = ioctl(lc->mTransformFd,G2D_CMD_LBC_ROT,(unsigned long)(&info));
            if (ret)
            {
                loge("g2d G2D_CMD_LBC_ROT fail\n");
            }

            //second rotate 90��->180��
            info.enc_is_lossy = pPictureIn->bIsLossy;
            info.dec_is_lossy = pPictureIn->bIsLossy;
            info.blt.src_image_h.bbuff = 1;
            info.blt.src_image_h.use_phy_addr = 1;
            info.blt.src_image_h.color = 0xff;
            info.blt.src_image_h.format = G2D_FORMAT_YUV420_PLANAR;//g2d_fmt_enh
            info.blt.src_image_h.laddr[0] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops, (void *)lc->mRotateExtraBufInfo.pPicture.pData0);
            info.blt.src_image_h.laddr[1] = 0;
            info.blt.src_image_h.laddr[2] = 0;
            info.blt.src_image_h.width = lc->mRotateExtraBufInfo.pPicture.nWidth;
            info.blt.src_image_h.height = lc->mRotateExtraBufInfo.pPicture.nHeight;
            info.blt.src_image_h.align[0] = 0;
            info.blt.src_image_h.align[1] = 0;
            info.blt.src_image_h.align[2] = 0;
            info.blt.src_image_h.clip_rect.x = 0;
            info.blt.src_image_h.clip_rect.y = 0;
            info.blt.src_image_h.clip_rect.w = lc->mRotateExtraBufInfo.pPicture.nWidth;
            info.blt.src_image_h.clip_rect.h = lc->mRotateExtraBufInfo.pPicture.nHeight;
            info.blt.src_image_h.gamut = G2D_BT709;
            info.blt.src_image_h.bpremul = 0;
            info.blt.src_image_h.alpha = 0xff;
            info.blt.src_image_h.mode = G2D_GLOBAL_ALPHA;
            info.blt.dst_image_h.bbuff = 1;
            info.blt.dst_image_h.use_phy_addr = 1;
            info.blt.dst_image_h.color = 0xff;
            info.blt.dst_image_h.format = G2D_FORMAT_YUV420_PLANAR;
            info.blt.dst_image_h.laddr[0] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureOut->pData0);
            info.blt.dst_image_h.laddr[1] = 0;
            info.blt.dst_image_h.laddr[2] = 0;
            info.blt.dst_image_h.width = pPictureOut->nWidth;
            info.blt.dst_image_h.height = pPictureOut->nHeight;
            info.blt.dst_image_h.align[0] = 0;
            info.blt.dst_image_h.align[1] = 0;
            info.blt.dst_image_h.align[2] = 0;
            info.blt.dst_image_h.clip_rect.x = 0;
            info.blt.dst_image_h.clip_rect.y = 0;
            info.blt.dst_image_h.clip_rect.w = pPictureOut->nWidth;
            info.blt.dst_image_h.clip_rect.h = pPictureOut->nHeight;
            info.blt.dst_image_h.gamut = G2D_BT709;
            info.blt.dst_image_h.bpremul = 0;
            info.blt.dst_image_h.alpha = 0xff;
            info.blt.dst_image_h.mode = G2D_GLOBAL_ALPHA;
            ret = ioctl(lc->mTransformFd,G2D_CMD_LBC_ROT,(unsigned long)(&info));
            if (ret)
            {
                loge("g2d G2D_CMD_LBC_ROT fail\n");
            }
        }
        else
        {
            info.enc_is_lossy = pPictureIn->bIsLossy;
            info.dec_is_lossy = pPictureIn->bIsLossy;
            info.blt.src_image_h.bbuff = 1;
            info.blt.src_image_h.use_phy_addr = 1;
            info.blt.src_image_h.color = 0xff;
            info.blt.src_image_h.format = G2D_FORMAT_YUV420_PLANAR;//g2d_fmt_enh
            info.blt.src_image_h.laddr[0] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureIn->pData0);
            info.blt.src_image_h.laddr[1] = 0;
            info.blt.src_image_h.laddr[2] = 0;
            info.blt.src_image_h.width = pPictureIn->nWidth;
            info.blt.src_image_h.height = pPictureIn->nHeight;
            info.blt.src_image_h.align[0] = 0;
            info.blt.src_image_h.align[1] = 0;
            info.blt.src_image_h.align[2] = 0;
            info.blt.src_image_h.clip_rect.x = 0;
            info.blt.src_image_h.clip_rect.y = 0;
            info.blt.src_image_h.clip_rect.w = pPictureIn->nWidth;
            info.blt.src_image_h.clip_rect.h = pPictureIn->nHeight;
            info.blt.src_image_h.gamut = G2D_BT709;
            info.blt.src_image_h.bpremul = 0;
            info.blt.src_image_h.alpha = 0xff;
            info.blt.src_image_h.mode = G2D_GLOBAL_ALPHA;
            info.blt.dst_image_h.bbuff = 1;
            info.blt.dst_image_h.use_phy_addr = 1;
            info.blt.dst_image_h.color = 0xff;
            info.blt.dst_image_h.format = G2D_FORMAT_YUV420_PLANAR;
            info.blt.dst_image_h.laddr[0] = (size_addr)SunxiMemGetPhysicAddressCpu(lc->pMemops,(void*)pPictureOut->pData0);
            info.blt.dst_image_h.laddr[1] = 0;
            info.blt.dst_image_h.laddr[2] = 0;
            info.blt.dst_image_h.width = pPictureOut->nWidth;
            info.blt.dst_image_h.height = pPictureOut->nHeight;
            info.blt.dst_image_h.align[0] = 0;
            info.blt.dst_image_h.align[1] = 0;
            info.blt.dst_image_h.align[2] = 0;
            info.blt.dst_image_h.clip_rect.x = 0;
            info.blt.dst_image_h.clip_rect.y = 0;
            info.blt.dst_image_h.clip_rect.w = pPictureOut->nWidth;
            info.blt.dst_image_h.clip_rect.h = pPictureOut->nHeight;
            info.blt.dst_image_h.gamut = G2D_BT709;
            info.blt.dst_image_h.bpremul = 0;
            info.blt.dst_image_h.alpha = 0xff;
            info.blt.dst_image_h.mode = G2D_GLOBAL_ALPHA;

            ret = ioctl(lc->mTransformFd,G2D_CMD_LBC_ROT,(unsigned long)(&info));
            if (ret)
            {
                loge("g2d G2D_CMD_LBC_ROT fail\n");
            }
        }
	}
#endif
    logd("***g2d query finish,ret:%d\n",ret);
    return ret;
}
#endif
