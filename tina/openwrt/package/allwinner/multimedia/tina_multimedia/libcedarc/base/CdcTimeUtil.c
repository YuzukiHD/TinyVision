/*
 * =====================================================================================
 *
 *       Filename:  CdcDebugUitl.c
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2020年10月21日 10时51分53秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  jilinglin
 *        Company:  allwinnertech.com
 *
 * =====================================================================================
 */
#include <sys/time.h>
#include <stdio.h>
#include "CdcTimeUtil.h"

s64 DBGetCurrentTime(void)
{
    struct timeval tv;
    s64 time;
    gettimeofday(&tv,NULL);
    time = tv.tv_sec*1000000 + tv.tv_usec;
    return time;
}
