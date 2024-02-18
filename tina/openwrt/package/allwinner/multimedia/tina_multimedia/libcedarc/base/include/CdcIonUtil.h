/*
 * =====================================================================================
 *
 *       Filename:  CdcIonUtil.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2020年06月09日 16时31分13秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *        Company:
 *
 * =====================================================================================
 */
#ifndef  CDC_CORE_ION_H
#define  CDC_CORE_ION_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <stdint.h>
#include "aw_ion.h"

typedef enum MEMORY_TYPE {
    MEMORY_NORMAL,
    MEMORY_IOMMU,
} MEMORY_TYPE;


//No.1 add before linux-5.4
unsigned long CdcIonGetPhyAddr(int fd, uintptr_t handle);
//No.2 add before linux-5.4
int CdcIonMap(int fd, uintptr_t handle);
//No.3 add before linux-5.4
int CdcIonImport(int fd, int share_fd, aw_ion_user_handle_t *ion_handle);
//No.4 add before linux-5.4
int CdcIonFree(int fd, aw_ion_user_handle_t ion_handle);
//No.5 add before linux-5.4
int CdcIonOpen(void);
//No.6 add before linux-5.4
int CdcIonClose(int fd);
//No.7 add before linux-5.4
int CdcIonMmap(int buf_fd, size_t len, unsigned char **pVirAddr);
//No.8 add before linux-5.4
int CdcIonMunmap(size_t len, unsigned char *pVirAddr);
//No.9 add before linux-5.4
int CdcIonGetMemType();
//No.10 add before linux-5.4
unsigned long CdcIonGetTEEAddr(int fd, uintptr_t handle);
//No.11 add before linux-5.4
int CdcIonGetTotalSize(int fd);
//No.12 add before linux-5.4
void CdcIonFlushCache(int fd, int flushAll, void* startAddr, int size);
//No.13 add before linux-5.4
int CdcIonAlloc(int fd, size_t len, size_t align, unsigned int heap_mask, unsigned int flags,
                 aw_ion_user_handle_t* handle);
//No.14 add before linux-5.4
int CdcIonShare(int fd, aw_ion_user_handle_t handle, int* share_fd);


//No.15 add in linux-5.4
int CdcIonAllocFd(int fd, size_t len, size_t align, unsigned int heap_mask,
                   unsigned int flags, int *handle_fd);
//No.16 add in linux-5.4
int CdcIonSyncFd(int fd, int handle_fd);
//No.17 add in linux-5.4
int CdcIonQueryHeapCnt(int fd, int* cnt);
//No.18 add in linux-5.4
int CdcIonQueryGetHeaps(int fd, int cnt, void* buffers);
//No.19 add in linux-5.4
int CdcIonIsLegacy(int fd);



#ifdef __cplusplus
}
#endif


#endif
