/*
 * =====================================================================================
 *
 *       Filename:  CdcSinkInterface.c
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2020年10月22日 15时51分05秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  jilinglin
 *        Company:  allwinnertech.com
 *
 * =====================================================================================
 */

#include "CdcSinkInterface.h"
#include "CdcPicSink.h"
#include "CdcBSSink.h"
#include "cdc_log.h"

CdcSinkInterface* CdcSinkIFCreate(int nType)
{
    CdcSinkInterface* pInterface = NULL;
    switch(nType)
    {
        case CDC_SINK_TYPE_PIC:
            pInterface = CdcPicSinkCreate();
            break;
        case CDC_SINK_TYPE_BS:
            pInterface = CdcBSSinkCreate();
            break;
        case CDC_SINK_TYPE_TIME:
            break;
        case CDC_SINK_TYPE_NORMAL:
            break;
        default:
            loge("not support sink type");
    }
    return pInterface;
}

void CdcSinkIFDestory(CdcSinkInterface* pSelf, int nType)
{
    switch(nType)
    {
        case CDC_SINK_TYPE_PIC:
            CdcPicSinkDestory(pSelf);
            break;
        case CDC_SINK_TYPE_BS:
            CdcBSSinkDestory(pSelf);
            break;
        case CDC_SINK_TYPE_TIME:
            break;
        case CDC_SINK_TYPE_NORMAL:
            break;
        default:
            loge("not support sink type");
    }
}
