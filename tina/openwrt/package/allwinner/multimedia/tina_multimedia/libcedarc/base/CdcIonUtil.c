/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : cdcionUtil.c
* Description : copy form android for ion
* Author : jilinglin@allwinnertech.com
*
* History :
* Comment :
*/

#include <errno.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include <stdatomic.h> //some compile not support
#include <unistd.h>

#include "cdc_log.h"
#include "aw_ion.h"
#include "CdcIonUtil.h"

#define UNUSA_PARAM(param) (void)param

enum ion_version { ION_VERSION_UNKNOWN, ION_VERSION_MODERN, ION_VERSION_LEGACY };

//static atomic_int g_ion_version = ATOMIC_VAR_INIT(ION_VERSION_UNKNOWN);

int CdcIonIsLegacy(int fd) {
    int version = ION_VERSION_UNKNOWN;
    //int version = atomic_load_explicit(&g_ion_version, memory_order_acquire);
    //if (version == ION_VERSION_UNKNOWN) {
        /**
          * Check for FREE IOCTL here; it is available only in the old
          * kernels, not the new ones.
          */
        //TODO: use another way or silient the error log in this call.
        int err = CdcIonFree(fd, (aw_ion_user_handle_t)NULL);
        version = (err == -ENOTTY) ? ION_VERSION_MODERN : ION_VERSION_LEGACY;
    //    atomic_store_explicit(&g_ion_version, version, memory_order_release);
    //}

    return version == ION_VERSION_LEGACY;
}

int CdcIonMap(int fd, uintptr_t handle)
{
    int ret;
    aw_ion_fd_data_t fd_data;

    if (!CdcIonIsLegacy(fd))
    {
        loge("ion kernel driver not support, you should use ohter func");
        return -1;
    }

    fd_data.handle = (aw_ion_user_handle_t)handle;
    ret = ioctl(fd, AW_MEM_ION_IOC_MAP, &fd_data);
    if(ret)
    {
        loge("CdcIonGetFd map other dev's handle err, ret %d, dmabuf fd 0x%08x\n",
                                                                ret, fd_data.aw_fd);
        return -1;
    }
    return fd_data.aw_fd;
}

int CdcIonImport(int fd, int share_fd, aw_ion_user_handle_t *ion_handle)
{
    int ret;
    aw_ion_fd_data_t fd_data;

    if (!CdcIonIsLegacy(fd))
    {
        loge("ion kernel driver not support, you should use ohter func");
        return -1;
    }

    memset(&fd_data, 0, sizeof(aw_ion_fd_data_t));
    fd_data.aw_fd = share_fd;
    ret = ioctl(fd, AW_MEM_ION_IOC_IMPORT, &fd_data);
    if(ret)
    {
        loge("CdcIonImport get ion_handle err, ret %d\n",ret);
        return -1;
    }

    *ion_handle = fd_data.handle;
    return 0;
}

int CdcIonShare(int fd, aw_ion_user_handle_t handle, int* share_fd)
{
    int ret;
    aw_ion_fd_data_t data = {
        .handle = handle,
    };

    if (!CdcIonIsLegacy(fd))
    {
        loge("ion kernel driver not support, you should use ohter func");
        return -1;
    }
    if (share_fd == NULL)
    {
        loge("fd pointer is null");
        return -1;
    }

    ret = ioctl(fd, AW_MEM_ION_IOC_SHARE, &data);
    if (ret < 0)
    {
        loge("share fail, ret:%d fd:%d", ret, data.aw_fd);
        return -1;
    }

    *share_fd = data.aw_fd;
    return ret;
}

int CdcIonAlloc(int fd, size_t len, size_t align, unsigned int heap_mask, unsigned int flags,
                 aw_ion_user_handle_t* handle)
{
    int ret = 0;

    if ((handle == NULL) || (!CdcIonIsLegacy(fd)))
    {
        loge("ion kernel driver not support, you should use ohter func");
        return -1;
    }
    aw_ion_allocation_data_t data = {
        .aw_len = len, .aw_align = align, .aw_heap_id_mask = heap_mask, .flags = flags,
    };

    ret = ioctl(fd, AW_MEM_ION_IOC_ALLOC, &data);
    if (ret < 0)
    {
        loge("CdcIonAlloc err, ret %d\n",ret);
        return -1;
    }

    *handle = data.handle;

    return ret;
}

int CdcIonAllocFd(int fd, size_t len, size_t align, unsigned int heap_mask, unsigned int flags,
                 int* handle_fd) {
    aw_ion_user_handle_t handle;
    int ret = 0;

    if (!handle_fd)
    {
        loge("fd pointer is null");
        return -1;
    }

    if (!CdcIonIsLegacy(fd)) {
        struct aw_ion_new_alloc_data data = {
            .len = len,
            .heap_id_mask = heap_mask,
            .flags = flags,
        };

        ret = ioctl(fd, AW_ION_IOC_NEW_ALLOC, &data);
        if (ret < 0)
        {
            loge("new alloc fail");
            return -1;
        }
        *handle_fd = data.fd;
    } else {
        ret = CdcIonAlloc(fd, len, align, heap_mask, flags, &handle);
        if (ret < 0)
        {
            loge("alloc fail");
            return -1;
        }
        ret = CdcIonShare(fd, handle, handle_fd);
        if (ret < 0)
        {
            loge("alloc fail");
            return -1;
        }
        ret = CdcIonFree(fd, handle);
    }
    return ret;
}

int CdcIonSyncFd(int fd, int handle_fd)
{
    int ret = 0;
    aw_ion_fd_data_t data = {
        .aw_fd = handle_fd,
    };

    if (!CdcIonIsLegacy(fd))
    {
        loge("ion kernel driver not support, you should use ohter func");
        return -1;
    }

    ret = ioctl(fd, AW_MEM_ION_IOC_SYNC, &data);
    if(ret < 0)
    {
        loge("CdcIonAlloc err, ret %d\n",ret);
        return -1;
    }
    return 0;
}

int CdcIonQueryHeapCnt(int fd, int* cnt) {
    int ret;
    struct aw_ion_heap_query query;

    if (!cnt)
    {
        loge("cnt pointer is null");
        return -1;
    }
    memset(&query, 0, sizeof(query));

    ret = ioctl(fd, AW_ION_IOC_HEAP_QUERY, &query);
    if (ret < 0)
    {
        loge("query fail\n");
        return -1;
    }
    *cnt = query.cnt;
    return ret;
}

int CdcIonQueryGetHeaps(int fd, int cnt, void* buffers) {
    int ret;
    struct aw_ion_heap_query query = {
        .cnt = cnt, .heaps = (uintptr_t)buffers,
    };

    ret = ioctl(fd, AW_ION_IOC_HEAP_QUERY, &query);
    if (ret < 0)
    {
        loge("get heaps fail\n");
        return -1;
    }
    return ret;
}


int CdcIonFree(int fd, aw_ion_user_handle_t ion_handle)
{
    int ret;
    aw_ion_handle_data_t sIonHandleData;

    memset(&sIonHandleData, 0, sizeof(aw_ion_handle_data_t));
    sIonHandleData.handle = ion_handle;
    ret = ioctl(fd, AW_MEM_ION_IOC_FREE, &sIonHandleData);
    if(ret < 0)
    {
        if(errno != ENOTTY)
            loge("free ion_handle err, ret %d errno:%d",ret, errno);
        return -errno;
    }
    return 0;
}

int CdcIonOpen(void)
{
    int fd = open("/dev/ion", O_RDWR);
    if (fd < 0)
        loge("open /dev/ion failed: %s\n",strerror(errno));

    return fd;
}

int CdcIonClose(int fd)
{
    int ret = close(fd);
    if (ret < 0)
    {
        loge("CdcIonClose failed with code %d: %s\n", ret, strerror(errno));
        return -errno;
    }
    return ret;
}

int CdcIonMmap(int buf_fd, size_t len, unsigned char **pVirAddr)
{
    /* mmap to user */
    *pVirAddr = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, buf_fd, 0);
    if(MAP_FAILED == pVirAddr)
    {
        loge("CdcIonMmap failed: %s\n", strerror(errno));
        return -errno;
    }

    return 0;
}

int CdcIonMunmap(size_t len, unsigned char *pVirAddr)
{
    int ret;
    ret = munmap((void*)pVirAddr, len);
    if(ret)
    {
        loge("CdcIonUnmap failed: %s\n", strerror(errno));
        return -errno;
    }

    return 0;
}

unsigned long CdcIonGetTEEAddr(int fd, uintptr_t handle)
{
    int ret = 0;
    aw_sunxi_phys_data_t tee_data;
    aw_ion_custom_data_t custom_data;

    if (!CdcIonIsLegacy(fd))
    {
        loge("ion kernel driver not support, you should use ohter func");
        return -1;
    }

    memset(&tee_data, 0, sizeof(aw_sunxi_phys_data_t));
    tee_data.handle = (aw_ion_user_handle_t)handle;
    custom_data.aw_cmd = ION_IOC_SUNXI_TEE_ADDR;
    custom_data.aw_arg = (unsigned long)&tee_data;
    ret = ioctl(fd, AW_MEM_ION_IOC_CUSTOM, &custom_data);
    if(ret)
    {
        loge("ION_IOC_CUSTOM get TEE addr err, ret %d\n", ret);
        return 0;
    }
    return tee_data.phys_addr;
}

int CdcIonGetMemType(void)
{
    MEMORY_TYPE eMemoryType = MEMORY_NORMAL;

#ifdef CONF_USE_IOMMU
    eMemoryType = MEMORY_IOMMU;
#else
    struct stat iommu_sysfs;
    if (stat("/sys/class/iommu", &iommu_sysfs) == 0 && S_ISDIR(iommu_sysfs.st_mode))
    {
        eMemoryType = MEMORY_IOMMU;
    }
#endif
    return eMemoryType;
}

unsigned long CdcIonGetPhyAddr(int fd, uintptr_t handle)
{

    unsigned long phy = 0;
    int ret = 0;
    aw_ion_custom_data_t custom_data;
    aw_sunxi_phys_data_t phys_data;

    if (!CdcIonIsLegacy(fd))
    {
        loge("ion kernel driver not support, you should use ohter func");
        return 0;
    }

    if(CdcIonGetMemType() != MEMORY_IOMMU)
    {

        memset(&phys_data, 0, sizeof(aw_sunxi_phys_data_t));
        custom_data.aw_cmd = ION_IOC_SUNXI_PHYS_ADDR;
        phys_data.handle = (aw_ion_user_handle_t)handle;
        custom_data.aw_arg = (unsigned long)&phys_data;
        ret = ioctl(fd, AW_MEM_ION_IOC_CUSTOM, &custom_data);
        if(ret < 0)
        {
            loge("CdcIonGetPhyAdr AW_MEM_ION_IOC_CUSTOM error\n");
            phy = 0;
        }
        phy = phys_data.phys_addr;
    }
    else
    {
        loge("mem type is iommu, we not support");
    }

    return phy;
}

//note: please check whether kernel support, i can't find kernel support now
//maybe in a branch which i don't konw, func in here just for compatible
int CdcIonGetTotalSize(int fd)
{
    int ret = 0;

    sunxi_pool_info_t binfo = {
        .total   = 0,    // mb
        .free_kb  = 0, //the same to free_mb
        .free_mb = 0,
    };

    aw_ion_custom_data_t cdata;

    if (!CdcIonIsLegacy(fd))
    {
        loge("ion kernel driver not support, you should use ohter func");
        return -1;
    }

    cdata.aw_cmd = ION_IOC_SUNXI_POOL_INFO;
    cdata.aw_arg = (unsigned long)&binfo;
    ret = ioctl(fd, AW_MEM_ION_IOC_CUSTOM, &cdata);
    if (ret < 0){
        logw("Failed to ioctl ion device, errno:%s\n", strerror(errno));
        return -1;
    }

    logd(" ion dev get free pool [%d MB], total [%d MB]\n", binfo.free_mb, binfo.total / 1024);
    ret = binfo.total;

    return ret;
}

void CdcIonFlushCache(int fd, int flushAll, void* startAddr, int size)
{
    aw_cache_range_t range;
    int ret;

#ifdef CONF_KERNEL_VERSION_3_4
    aw_ion_custom_data_t custom_data;
    if(flushAll)
    {
        ret = ioctl(fd, ION_IOC_SUNXI_FLUSH_ALL, 0);
    }
    else
    {
        range.start = (unsigned long)startAddr;
        range.end = (unsigned long)startAddr + size;

        custom_data.aw_cmd = ION_IOC_SUNXI_FLUSH_RANGE;
        custom_data.aw_arg = (unsigned long)&range;
        ret = ioctl(fd, AW_MEM_ION_IOC_CUSTOM, &custom_data);
    }
    if (ret)
    {
        loge("ION_IOC_CUSTOM failed\n");
    }
#elif (defined CONF_KERNEL_VERSION_3_10) || (defined CONF_KERNEL_VERSION_4_9)
    if(flushAll)
    {
        ret = ioctl(fd, ION_IOC_SUNXI_FLUSH_ALL, 0);
    }
    else
    {
        range.start = (unsigned long)startAddr;
        range.end = (unsigned long)startAddr + size;
        logv("start:%p, end:%llx, size:%lx(%ld)\n", startAddr, range.end, (long)size, (long)size);
        ret = ioctl(fd, ION_IOC_SUNXI_FLUSH_RANGE, &range);
    }
    if (ret)
    {
        loge("ION_IOC_SUNXI_FLUSH_RANGE failed errno: %d, ret: %d", errno, ret);
    }
#else
    UNUSA_PARAM(range);
    UNUSA_PARAM(fd);
    UNUSA_PARAM(ret);
    UNUSA_PARAM(flushAll);
    UNUSA_PARAM(startAddr);
    UNUSA_PARAM(size);
    loge("this kernel version we no support");

#endif

    return;
}
