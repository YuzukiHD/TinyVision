/*
 * =====================================================================================
 *
 *       Filename:  CdcPicSink.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2020年10月22日 16时07分57秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  jilinglin
 *        Company:
 *
 * =====================================================================================
 */
#ifndef CDC_PIC_SINK
#define CDC_PIC_SINK

#include "CdcSinkInterface.h"

CdcSinkInterface* CdcPicSinkCreate(void);

void CdcPicSinkDestory(CdcSinkInterface* pSelf);

#endif
