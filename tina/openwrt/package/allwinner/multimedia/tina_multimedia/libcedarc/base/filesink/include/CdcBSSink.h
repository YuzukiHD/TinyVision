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
#ifndef CDC_BS_SINK
#define CDC_BS_SINK

#include "CdcSinkInterface.h"

CdcSinkInterface* CdcBSSinkCreate(void);

void CdcBSSinkDestory(CdcSinkInterface* pSelf);

#endif
