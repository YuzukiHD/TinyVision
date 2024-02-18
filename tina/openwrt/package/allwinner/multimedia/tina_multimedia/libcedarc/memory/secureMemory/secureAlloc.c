/*
* Copyright (c) 2020-2028 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : secureAlloc.c
* Description :
* History :
*   Author  : ganqiuye <ganqiuye@allwinnertech.com>
*   Date    : 2020/10/23
*   Comment :
*
*/

#if (PLATFORM_SURPPORT_SECURE_OS== 1)
#define LOG_TAG "secureAlloc"
#include "cdc_log.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "sc_interface.h"
#include "secureAllocEntry.h"
#include "CdcMemList.h"
#include <tee_client_api.h>
#include "uapi-heap.h"
#include "veAdapter.h"
#include "veInterface.h"
#include "CdcIonUtil.h"

#if ((defined CONF_KERNEL_VERSION_4_9) || \
     (defined CONF_KERNEL_VERSION_4_4) || \
     (defined CONF_KERNEL_VERSION_3_10))
#define KERNEL_VERSION_OLD 1
#endif


#define SE_ASSERT(expr) \
    do {\
        if(!(expr)) {\
            loge("%s:%d, assert \"%s\" failed", __FILE__, __LINE__, #expr); \
            abort();\
        }\
    } while(0)

enum {
    /* keep these values (started with MSG_ADAPTER_) in sync
     * with definitions in vdecoder in secureos*/
    MSG_ADPATER_MIN = 0,
    MSG_ADPATER_INIT,
    MSG_ADPATER_MEM_ALLOC,
    MSG_ADPATER_MEM_FREE,
    MSG_ADPATER_MEM_COPY,
    MSG_ADPATER_MEM_SET,
    MSG_ADPATER_MEM_FLUSH_CACHE,
    MSG_ADPATER_MEM_GET_PHYSICAL_ADDRESS,
    MSG_ADPATER_MEM_GET_VIRTUAL_ADDRESS,
    MSG_ADPATER_MEM_READ,
    MSG_ADPATER_MEM_WRITE,
    MSG_ADPATER_MEM_READ_INT,
    MSG_ADPATER_MEM_WRITE_INT,
    MSG_ADPATER_MEM_DUMP,
    MSG_ADPATER_MEM_DEBUG,
    MSG_ADPATER_EXIT,
    MSG_ADPATER_SETUP_VE,
    MSG_ADPATER_SHUTDOWN_VE,
    MSG_ADPATER_MAX,
};

typedef struct SECURE_BUFFER_NODE
{
    struct aw_mem_list_head i_list;
    unsigned long phy_addr;        //phisical address
    unsigned int size;        //buffer size
    unsigned int tee_addr;
    unsigned int buf_fd;
}sSecureBufferNode;


typedef struct {
    int                        heap_fd;     // secure heap driver handle
    TEEC_Context               tee_context;
    TEEC_Session               tee_session;
    int                        ref_count;
    unsigned int               phyOffset;
    struct aw_mem_list_head    list;
    VeOpsS*                    ve_ops;
    void*                      ve_self;
    int                        inter_ve;
}sSecureMemContext;

static sSecureMemContext *gAdapterContext = NULL;
static pthread_mutex_t gAdapterMutex = PTHREAD_MUTEX_INITIALIZER;

static const uint8_t adapter_UUID[16] = {
    0xEA, 0xD2, 0x78, 0x4D, 0x31, 0xA6,    0xFB, 0x70,
    0xAA, 0xA7,    0x87, 0xC2, 0xB5, 0x77, 0x30, 0x52
};

/*Note: the /dev/sunxi_drm_heap is a NOT standard dev
*       it will be removed in one day
*       then the code should be changed sync to kernel
*/
static int dmabuf_heap_open(void)
{
    int fd = -1;
#if KERNEL_VERSION_OLD
    fd = CdcIonOpen();
#else
    fd = open("/dev/sunxi_drm_heap", O_RDWR);
    if (fd < 0)
        loge("open /dev/sunxi_drm_heap failed: %s\n",strerror(errno));
    else
        logd("open /dev/sunxi_drm_heap success!!!");
#endif
    return fd;
}



static int SecureAllocOpen2(void* veops, void* veself)
{
    //logd("HwSecureAllocOpen, pid %d", getpid());
    pthread_mutex_lock(&gAdapterMutex);
    TEEC_Result err = TEEC_SUCCESS;
    if (gAdapterContext != NULL)
    {
        gAdapterContext->ref_count++;
        logd("SecureAllocOpen already open, ref_count:%d\n",gAdapterContext->ref_count);
        pthread_mutex_unlock(&gAdapterMutex);
        return gAdapterContext->ref_count;
    }
    else
    {
        gAdapterContext = (sSecureMemContext *)malloc(sizeof(sSecureMemContext));
        SE_ASSERT(gAdapterContext != NULL);
        memset(gAdapterContext, 0, sizeof(sSecureMemContext));
        gAdapterContext->phyOffset = CdcVeGetPhyOffset(veops, veself);

        gAdapterContext->ve_ops  = veops;
        gAdapterContext->ve_self = veself;
        gAdapterContext->heap_fd   = dmabuf_heap_open();
        if(gAdapterContext->heap_fd < 0)
            goto ERROR_OUT;

        err = TEEC_InitializeContext(NULL, &gAdapterContext->tee_context);
        if(err == TEEC_SUCCESS) {
            err = TEEC_OpenSession(&gAdapterContext->tee_context, &gAdapterContext->tee_session,
                    (const TEEC_UUID *)&adapter_UUID[0], TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);

            if(err == TEEC_SUCCESS) {
                //session opened successfully, init adapter.
                TEEC_Operation operation;
                memset(&operation, 0, sizeof(TEEC_Operation));

                operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
                        TEEC_NONE, TEEC_NONE);

                operation.started = 1;
                err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
                        MSG_ADPATER_INIT, &operation, NULL);
                logw("secure memory adapter has been created.");
            }
        }

        if(err == 0)
        {
            gAdapterContext->ref_count++;
            AW_MEM_INIT_LIST_HEAD(&gAdapterContext->list);
        }
        else
        {
            loge("initialize context failed, %d", err);
            close(gAdapterContext->heap_fd);
            goto ERROR_OUT;
        }
    }
    pthread_mutex_unlock(&gAdapterMutex);
    return 0;
ERROR_OUT:
    free(gAdapterContext);
    gAdapterContext = NULL;
    pthread_mutex_unlock(&gAdapterMutex);
    return -1;
}

static int SecureAllocOpen()
{
    pthread_mutex_lock(&gAdapterMutex);
    if (gAdapterContext != NULL)
    {
        gAdapterContext->ref_count++;
        logd("SecureAllocOpen already open, ref_count:%d\n",gAdapterContext->ref_count);
        pthread_mutex_unlock(&gAdapterMutex);
        return gAdapterContext->ref_count;
    }
    pthread_mutex_unlock(&gAdapterMutex);
    int type = VE_OPS_TYPE_NORMAL;
    VeOpsS* veOps = GetVeOpsS(type);
    if(veOps == NULL)
    {
        loge("get ve ops failed");
        return -1;
    }
    VeConfig mVeConfig;
    memset(&mVeConfig, 0, sizeof(VeConfig));
    mVeConfig.nDecoderFlag = 1;
    mVeConfig.bNotSetVeFreq = 1;

    void* pVeopsSelf = CdcVeInit(veOps,&mVeConfig);
    if(pVeopsSelf == NULL)
    {
        loge("init ve ops failed");
        CdcVeRelease(veOps, pVeopsSelf);
        return -1;
    }
    if(SecureAllocOpen2(veOps, pVeopsSelf) != 0)
    {
        CdcVeRelease(veOps, pVeopsSelf);
        return -1;
    }
    pthread_mutex_lock(&gAdapterMutex);
    gAdapterContext->inter_ve = 1;
    pthread_mutex_unlock(&gAdapterMutex);
    return 0;
}


static void SecureAllocClose()
{
    //logd("HwSecureAllocClose, pid %d", getpid());
    pthread_mutex_lock(&gAdapterMutex);
    if(!gAdapterContext) {
        loge("secure adapter has not been initialized");
        pthread_mutex_unlock(&gAdapterMutex);
        return ;
    }
    TEEC_Result err = 0;
    if(--gAdapterContext->ref_count == 0) {
        TEEC_Operation operation;
        memset(&operation, 0, sizeof(TEEC_Operation));

        operation.started = 1;
        operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
        err  = TEEC_InvokeCommand(&gAdapterContext->tee_session,
                MSG_ADPATER_EXIT, &operation, NULL);
        if (err != TEEC_SUCCESS) {
            loge("call invoke command error");
        }
        TEEC_CloseSession(&gAdapterContext->tee_session);
        TEEC_FinalizeContext(&gAdapterContext->tee_context);
        struct aw_mem_list_head * pos, *q;
        aw_mem_list_for_each_safe(pos, q, &gAdapterContext->list)
        {
            sSecureBufferNode * tmp;
            tmp = aw_mem_list_entry(pos, sSecureBufferNode, i_list);
            logv("SecureAllocClose del item phy= 0x%lx vir= 0x%lx, size= %d\n",
                tmp->phy_addr, tmp->tee_addr, tmp->size);
            close(tmp->buf_fd);
            aw_mem_list_del(pos);
            free(tmp);
        }

        close(gAdapterContext->heap_fd);
        if(gAdapterContext->inter_ve)
            CdcVeRelease(gAdapterContext->ve_ops, gAdapterContext->ve_self);
        free(gAdapterContext);
        gAdapterContext = NULL;
        logw("secure memory adapter has been destroyed.");
    }
    pthread_mutex_unlock(&gAdapterMutex);
    return ;
}

static int SecureAllocGetTotalSize()
{
    loge("SecureAllocGetTotalSize");
    return 0;
}

#if KERNEL_VERSION_OLD
static int dmabuf_get_phy_addr(int nIonFd, int nBufFd, unsigned long *pAddr)
{
    struct user_iommu_param nIommuBuffer;
    unsigned long phy = 0;
    int ret = 0;
    aw_ion_user_handle_t ion_handle;

    if(CdcIonGetMemType() == MEMORY_IOMMU)
    {
        nIommuBuffer.fd = nBufFd;
        CdcVeGetIommuAddr(gAdapterContext->ve_ops, gAdapterContext->ve_self, &nIommuBuffer);

        if(nIommuBuffer.iommu_addr & 0xff)
        {
            loge("get iommu addr maybe wrong:%x", nIommuBuffer.iommu_addr);
            return -1;
        }
        logv("ion_alloc_palloc: fd:%d, iommu_addr:%x", nIommuBuffer.fd,
            nIommuBuffer.iommu_addr);

        *pAddr = (unsigned long)nIommuBuffer.iommu_addr;
    }
    else
    {
        ret = CdcIonImport(nIonFd, nBufFd, &ion_handle);
        if(ret < 0)
        {
            loge("import fail");
            return -1;
        }

        phy = CdcIonGetPhyAddr(nIonFd, ion_handle);
        if(ret <= 0)
        {
            loge("get phy fail");
            return -1;
        }

        *pAddr = phy;
    }
    return 0;
}
static void* SecureAllocPalloc(int size, void*veOps, void* pVeopsSelf)
{
    unsigned int heap_mask;
    unsigned int flags;
    int dma_buf_fd;
    unsigned long addr_phy = 0;
    unsigned char *addr_vir = NULL;
    unsigned long addr_tee = 0;
    sSecureBufferNode *alloc_buffer = NULL;
    aw_ion_user_handle_t ion_handle;
    int ret = 0;

    (void)veOps;
    (void)pVeopsSelf;

    pthread_mutex_lock(&gAdapterMutex);

    if (gAdapterContext == NULL)
    {
        loge("ion_alloc do not opened, should call ion_alloc_open() \
            before ion_alloc_alloc(size)\n");
        goto ALLOC_OUT;
    }

    if(size <= 0)
    {
        logv("can not alloc size 0\n");
        goto ALLOC_OUT;
    }

    heap_mask = AW_SECURE_HEAP_MASK;
    flags     = AW_ION_CACHED_FLAG | AW_ION_CACHED_NEEDS_SYNC_FLAG;

    ret = CdcIonAllocFd(gAdapterContext->heap_fd, size, SZ_4k, heap_mask, flags, &dma_buf_fd);

    if(ret < 0)
    {
        loge("alloc fd fail\n");
        goto ALLOC_OUT;
    }

    ret = CdcIonMmap(dma_buf_fd, size, &addr_vir);
    if(ret < 0)
    {
        loge("mmap fd fail\n");
        goto ALLOC_OUT;
    }


#if(ADJUST_ADDRESS_FOR_SECURE_OS_OPTEE)
    ret = CdcIonImport(gAdapterContext->heap_fd, dma_buf_fd, &ion_handle);
    if(ret < 0)
    {
        loge("import fail");
        goto ALLOC_OUT;
    }
    addr_tee = CdcIonGetTEEAddr(gAdapterContext->heap_fd, ion_handle);

#else
    UNUSA_PARAM(ion_handle);
    addr_tee = (unsigned long)addr_vir;
#endif


    ret = dmabuf_get_phy_addr(gAdapterContext->heap_fd, dma_buf_fd, &addr_phy);
    if(ret < 0)
    {
        loge("get phy addr error\n");
        CdcIonMunmap(size, addr_vir);
        goto ALLOC_OUT;
    }

    alloc_buffer = (sSecureBufferNode *)malloc(sizeof(sSecureBufferNode));
    if (alloc_buffer == NULL)
    {
        loge("malloc buffer node failed");

        addr_phy = 0;
        addr_vir = 0;        // value of MAP_FAILED is -1, should return 0

        goto ALLOC_OUT;
    }

    alloc_buffer->size      = size;
    alloc_buffer->phy_addr  = addr_phy;
    alloc_buffer->tee_addr  = addr_tee;
    alloc_buffer->buf_fd    = dma_buf_fd;
    aw_mem_list_add_tail(&alloc_buffer->i_list, &gAdapterContext->list);

ALLOC_OUT:

    pthread_mutex_unlock(&gAdapterMutex);
    return (void*)addr_tee;
}

#else
static int dmabuf_heap_alloc(int fd, size_t len, unsigned int heap_flags, int *dmabuf_fd)
{
    struct dma_heap_allocation_data data = {
        .len = len,
        .fd = 0,
        .fd_flags = O_RDWR | O_CLOEXEC,
        .heap_flags = heap_flags,
    };
    int ret;

    if (!dmabuf_fd)
        return -EINVAL;

    ret = ioctl(fd, DMA_HEAP_IOC_ALLOC, &data);
    if (ret < 0)
        return ret;
    *dmabuf_fd = (int)data.fd;
    return ret;
}

static int getPhyAddr(int dmabuf_fd, unsigned long *pAddr)
{
    struct user_iommu_param nIommuBuffer;
    nIommuBuffer.fd = dmabuf_fd;
    CdcVeGetIommuAddr(gAdapterContext->ve_ops, gAdapterContext->ve_self, &nIommuBuffer);
    if(nIommuBuffer.iommu_addr & 0xff)
    {
        loge("get iommu addr maybe wrong:%x", nIommuBuffer.iommu_addr);
        return -1;
    }

    *pAddr = (unsigned long)nIommuBuffer.iommu_addr;
    return 0;
}

static void *SecureAllocPalloc(int size, void *veOps, void *pVeopsSelf)
{
    (void)veOps;
    (void)pVeopsSelf;
    int dmabuf_fd = -1;
    unsigned long addr_vir = 0;
    unsigned long addr_phy = 0;
    sSecureBufferNode * alloc_buffer = NULL;
    pthread_mutex_lock(&gAdapterMutex);
    if (gAdapterContext == NULL)
    {
        loge("ion_alloc do not opened");
        goto ALLOC_OUT;
    }
    logv("SecureAllocPalloc, size %d", size);
    if(size <= 0)
    {
        goto ALLOC_OUT;
    }
    int ret = dmabuf_heap_alloc(gAdapterContext->heap_fd, size, 0, &dmabuf_fd);
    if (ret != 0)
    {
        loge("Allocation Failed with %d\n", ret);
        goto ALLOC_OUT;
    }
    alloc_buffer = (sSecureBufferNode *)malloc(sizeof(sSecureBufferNode));
    if (alloc_buffer == NULL)
    {
        loge("malloc buffer node failed");
        close(dmabuf_fd);
        goto ALLOC_OUT;
    }
    struct sunxi_drm_phys_data buffer_data;
    buffer_data.handle = (int)dmabuf_fd;
    ret = ioctl(gAdapterContext->heap_fd, DMA_HEAP_GET_ADDR, &buffer_data);
    if (ret != 0)
    {
        loge("dmabuf_heap_get_addr err!");
        goto ALLOC_OUT;
    }
    getPhyAddr(dmabuf_fd, &addr_phy);
    alloc_buffer->size        = size;
    alloc_buffer->phy_addr    = addr_phy;
    alloc_buffer->tee_addr    = buffer_data.tee_addr;
    addr_vir                  = buffer_data.tee_addr;
    alloc_buffer->buf_fd      = dmabuf_fd;
    aw_mem_list_add_tail(&alloc_buffer->i_list, &gAdapterContext->list);
ALLOC_OUT:
    pthread_mutex_unlock(&gAdapterMutex);
    return (void*)addr_vir;
}
#endif

static void SecureAllocPfree(void *pVir, void *veOps, void *pVeopsSelf)
{
    (void)veOps;
    (void)pVeopsSelf;
    logv("SecureMemAdapterFree, pVir %p", pVir);
    if(pVir == NULL) {
        return ;
    }
    int flag = 0;
    unsigned long addr_vir = (unsigned long)pVir;
    sSecureBufferNode * tmp;
    pthread_mutex_lock(&gAdapterMutex);
    if (gAdapterContext == NULL)
    {
        loge("secure_alloc do not opened");
        pthread_mutex_unlock(&gAdapterMutex);
        return;
    }
    aw_mem_list_for_each_entry(tmp, &gAdapterContext->list, i_list)
    {
        if (tmp->tee_addr == addr_vir)
        {
            logv("SecureAllocPfree item phy= 0x%lx vir= 0x%lx, size= %d\n",
                tmp->phy_addr, tmp->tee_addr, tmp->size);
            /*close dma buffer fd*/
            close(tmp->buf_fd);
            /* free buffer */
            aw_mem_list_del(&tmp->i_list);
            free(tmp);
            flag = 1;
            break;
        }
    }

    if (0 == flag)
    {
        loge("ion_alloc_free failed, do not find virtual address: 0x%lx\n", addr_vir);
    }

    pthread_mutex_unlock(&gAdapterMutex);
    return ;
}

static void SecureAllocFlushCache(void *pVir, int size)
{
    logv("SecureAllocFlushCache, %p ", pVir);
    if(pVir == NULL || size <= 0) {
        return ;
    }
    SE_ASSERT(gAdapterContext != NULL);

    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT, TEEC_NONE, TEEC_NONE);
    operation.started = 1;
    operation.params[0].value.a = (uintptr_t)(pVir);
    operation.params[1].value.a = (uint32_t)size;
    TEEC_Result err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
            MSG_ADPATER_MEM_FLUSH_CACHE, &operation, NULL);
    logv("TEEC_InvokeCommand, err = %d",err);
    if(err == TEEC_ERROR_TARGET_DEAD)
        abort();
    return ;
}

static void* SecureAllocGetPhysicAddressCpu(void *pVir)
{
    int flag = 0;
    unsigned long addr_vir = (unsigned long)pVir;
    unsigned long addr_phy = 0;
    sSecureBufferNode * tmp;

    if (NULL == pVir)
    {
        return NULL;
    }

    pthread_mutex_lock(&gAdapterMutex);

    aw_mem_list_for_each_entry(tmp, &gAdapterContext->list, i_list)
    {
        if (addr_vir >= tmp->tee_addr
            && addr_vir < tmp->tee_addr + tmp->size)
        {
            addr_phy = tmp->phy_addr + addr_vir - tmp->tee_addr;
            flag = 1;
            break;
        }
    }

    if (0 == flag)
    {
        loge("SecureAllocGetPhysicAddressCpu failed, do not find virtual address: 0x%lx\n", addr_vir);
    }

    pthread_mutex_unlock(&gAdapterMutex);

    return (void*)addr_phy;
}

static void* SecureAllocGetVirtualAddressCpu(void *pPhy)
{
    int flag = 0;
    unsigned long addr_vir = 0;
    unsigned long addr_phy = (unsigned long)pPhy;
    sSecureBufferNode * tmp;

    if (NULL == pPhy)
    {
        loge("can not phy2vir NULL buffer\n");
        return NULL;
    }

    pthread_mutex_lock(&gAdapterMutex);

    aw_mem_list_for_each_entry(tmp, &gAdapterContext->list, i_list)
    {
        if (addr_phy >= tmp->phy_addr
            && addr_phy < tmp->phy_addr + tmp->size)
        {
            addr_vir = tmp->tee_addr + addr_phy - tmp->phy_addr;
            flag = 1;
            break;
        }
    }

    if (0 == flag)
    {
        loge("SecureAllocGetVirtualAddressCpu failed, do not find physical address: 0x%lx\n", addr_phy);
    }

    pthread_mutex_unlock(&gAdapterMutex);

    return (void*)addr_vir;
}

static void* SecureAllocGetPhysicAddressVE(void *pVir)
{
    logv("** phy offset = %x",gAdapterContext->phyOffset);

    return (void*)((unsigned long)SecureAllocGetPhysicAddressCpu(pVir) - gAdapterContext->phyOffset);
}

static void* SecureAllocGetVirtualAddressVE(void *pPhy)
{
    return (void*)((unsigned long)SecureAllocGetVirtualAddressCpu(pPhy) - gAdapterContext->phyOffset);
}

static int SecureAllocSet(void *s, int c, size_t n)
{
    logv("SecureAllocSet, s %p", s);
    if(s == NULL || n == 0) {
        return -1;
    }
    SE_ASSERT(gAdapterContext != NULL);

    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT, TEEC_VALUE_INPUT, TEEC_NONE);
    operation.started = 1;
    operation.params[0].value.a = (uintptr_t)(s);
    operation.params[1].value.a = (uint32_t)c;
    operation.params[2].value.a = (uint32_t)n;

    TEEC_Result err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
            MSG_ADPATER_MEM_SET, &operation, NULL);
    if(err == TEEC_ERROR_TARGET_DEAD)
        abort();
    return err;
}

static int SecureAllocCopy(void *dest, void *src, size_t n)
{
    if(dest == NULL || src == NULL) {
        return -1;
    }

    SE_ASSERT(gAdapterContext != NULL);

    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT, TEEC_VALUE_INPUT, TEEC_NONE);
    operation.started = 1;
    operation.params[0].value.a = (uintptr_t)dest;
    operation.params[1].value.a = (uintptr_t)src;
    operation.params[2].value.a = (uint32_t)n;
    TEEC_Result err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
            MSG_ADPATER_MEM_COPY, &operation, NULL);
    if(err == TEEC_ERROR_TARGET_DEAD)
        abort();
    return err;
}

static int SecureAllocRead(void *dest, void *src, size_t n)
{
    logv("SecureAllocRead");
    if(src == NULL || dest == 0 || n == 0) {
        return 0;
    }
    SE_ASSERT(gAdapterContext != NULL);

    TEEC_Operation operation;

    memset(&operation, 0, sizeof(TEEC_Operation));
    operation.started = 1;

    operation.params[0].value.a = (uintptr_t)src;
    operation.params[2].value.a = (uint32_t)n;

    TEEC_Result err;
    if (n > 4) {
        TEEC_SharedMemory share_mem;
        share_mem.size = n;
        share_mem.flags = TEEC_MEM_OUTPUT ;
        if(TEEC_AllocateSharedMemory(&gAdapterContext->tee_context, &share_mem) != TEEC_SUCCESS) {
            loge("allocate share memory fail");
            return 0;
        }
        operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
                TEEC_MEMREF_WHOLE, TEEC_VALUE_INPUT, TEEC_NONE);

        operation.params[1].memref.parent = &share_mem;
        operation.params[1].memref.offset = 0;
        operation.params[1].memref.size   = 0;

        err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
                MSG_ADPATER_MEM_READ, &operation, NULL);

        memcpy(dest, share_mem.buffer, n);
        TEEC_ReleaseSharedMemory(&share_mem);
    } else {
        operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
                TEEC_VALUE_OUTPUT, TEEC_VALUE_INPUT, TEEC_NONE);
        err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
                MSG_ADPATER_MEM_READ_INT, &operation, NULL);

        memcpy(dest, (unsigned char*)(&operation.params[1].value.a), n);
    }
    if(err == TEEC_ERROR_TARGET_DEAD)
        abort();
    return err;
}

static int SecureAllocWrite(void *dest, void *src, size_t n)
{
    logv("SecureAllocWrite");
    if(src == NULL || dest == 0 || n == 0) {
        return 0;
    }
    SE_ASSERT(gAdapterContext != NULL);

    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    operation.params[1].value.a = (uintptr_t)dest;
    operation.params[2].value.a = (uint32_t)n;

    TEEC_Result err;
    if (n > 4) {
        TEEC_SharedMemory share_mem;
        share_mem.size = n;
        share_mem.flags = TEEC_MEM_INPUT;
        if(TEEC_AllocateSharedMemory(&gAdapterContext->tee_context, &share_mem) != TEEC_SUCCESS) {
            loge("allocate share memory fail");
            return 0;
        }
        memcpy(share_mem.buffer, src, n);

        operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE,
                TEEC_VALUE_INPUT, TEEC_VALUE_INPUT, TEEC_NONE);
        operation.started = 1;

        operation.params[0].memref.parent = &share_mem;
        operation.params[0].memref.offset = 0;
        operation.params[0].memref.size   = 0;

        err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
                MSG_ADPATER_MEM_WRITE, &operation, NULL);
        TEEC_ReleaseSharedMemory(&share_mem);
    } else {
        operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
                TEEC_VALUE_INPUT, TEEC_VALUE_INPUT, TEEC_NONE);

        operation.params[0].value.a = *((uint32_t*)src);
        err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
                MSG_ADPATER_MEM_WRITE_INT, &operation, NULL);
    }
    if(err == TEEC_ERROR_TARGET_DEAD)
        abort();
    return err;
}

static int SecureAllocSetup()
{
    logv("SecureAdapterSetup");
    pthread_mutex_lock(&gAdapterMutex);
    if(gAdapterContext == NULL) {
        pthread_mutex_unlock(&gAdapterMutex);
        return -1;
    }
    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
            TEEC_NONE, TEEC_NONE);

    operation.started = 1;
    TEEC_Result err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
            MSG_ADPATER_SETUP_VE, &operation, NULL);
    pthread_mutex_unlock(&gAdapterMutex);
    if(err == TEEC_ERROR_TARGET_DEAD)
        abort();
    return err;
}

static int SecureAllocShutdown()
{
    logv("SecureAdapterShutdown");
    pthread_mutex_lock(&gAdapterMutex);
    //do not abort here, so codec can shutdown drm protection
    //without caring whether secure os is initialized or not.
    if(gAdapterContext == NULL) {
        pthread_mutex_unlock(&gAdapterMutex);
        return -1;
    }
    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
            TEEC_NONE, TEEC_NONE);

    operation.started = 1;
    TEEC_Result err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
            MSG_ADPATER_SHUTDOWN_VE, &operation, NULL);
    pthread_mutex_unlock(&gAdapterMutex);
    if(err == TEEC_ERROR_TARGET_DEAD)
        abort();
    return err;
}


static int SecureAllocGetVirByFd(int dmafd, void* viraddr)
{
    struct sunxi_drm_phys_data buffer_data;
    unsigned long* tmp_vir = (unsigned long*)viraddr;
    buffer_data.handle = (int)dmafd;
    int ret = ioctl(gAdapterContext->heap_fd, DMA_HEAP_GET_ADDR, &buffer_data);
    if (ret != 0)
    {
        loge("dmabuf_heap_get_addr err!");
        return -1;
    }
    *tmp_vir  = buffer_data.tee_addr;
    return 0;
}

static int SecureAllocGetPhyByFd(int dmafd, void* phyaddr)
{
    struct user_iommu_param nIommuBuffer;
    unsigned int* tmp_phy = (unsigned int*)phyaddr;
    if(gAdapterContext == NULL)
    {
        loge("context is null");
        return -1;
    }

    nIommuBuffer.fd = dmafd;

    CdcVeGetIommuAddr(gAdapterContext->ve_ops, gAdapterContext->ve_self, &nIommuBuffer);
    if(nIommuBuffer.iommu_addr & 0xff)
    {
        loge("get iommu addr maybe wrong:%x", nIommuBuffer.iommu_addr);
        return -1;
    }

    *tmp_phy = nIommuBuffer.iommu_addr;

    return 0;
}

static int SecureAllocFreePhyByFd(int dmafd, unsigned long phy)
{
    struct user_iommu_param nIommuBuffer;

    if(gAdapterContext == NULL)
    {
        loge("context is null");
        return -1;
    }
    nIommuBuffer.fd = dmafd;
    nIommuBuffer.iommu_addr = phy;
    logv("ion pfree: fd:%d, iommu_addr:%x\n", dmafd, phy);

    if(CdcVeFreeIommuAddr(gAdapterContext->ve_ops, gAdapterContext->ve_self, &nIommuBuffer) < 0)
    {
        loge("VeFreeIommuAddr error\n");
        return -1;
    }
    return 0;
}

struct ScMemOpsS _SecureMemOpsS =
{
    .open              = SecureAllocOpen,
    .close             = SecureAllocClose,
    .total_size        = SecureAllocGetTotalSize,
    .palloc            = SecureAllocPalloc,
    .pfree             = SecureAllocPfree,
    .flush_cache       = SecureAllocFlushCache,
    .ve_get_phyaddr    = SecureAllocGetPhysicAddressVE,
    .ve_get_viraddr    = SecureAllocGetVirtualAddressVE,
    .cpu_get_phyaddr   = SecureAllocGetPhysicAddressCpu,
    .cpu_get_viraddr   = SecureAllocGetVirtualAddressCpu,
    .mem_set           = SecureAllocSet,
    .mem_cpy           = SecureAllocCopy,
    .mem_read          = SecureAllocRead,
    .mem_write         = SecureAllocWrite,
    .setup             = SecureAllocSetup,
    .shutdown          = SecureAllocShutdown,
    .get_vir_by_fd     = SecureAllocGetVirByFd,
    .get_phy_by_fd     = SecureAllocGetPhyByFd,
    .free_phy_by_fd    = SecureAllocFreePhyByFd
};

struct ScMemOpsS* __GetSecureMemOpsS()
{
    logd("*** __GetSecureMemOpsS ***");
    return &_SecureMemOpsS;
}
#endif // ifdef PLATFORM_SURPPORT_SECURE_OS
