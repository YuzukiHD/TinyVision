/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : ionAlloc.c
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/


/*
 * ion_alloc.c
 *
 * john.fu@allwinnertech.com
 *
 * ion memory allocate
 *
 */

//#define CONFIG_LOG_LEVEL    OPTION_LOG_LEVEL_DETAIL
#define LOG_TAG "ionAlloc"

#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "cdc_log.h"
#include "CdcIonUtil.h"
#include "CdcMemList.h"
#include "ionAllocEntry.h"
#include "sc_interface.h"

#include "veAdapter.h"
#include "veInterface.h"

//* should include other  '*.h'  before CdcMalloc.h
#include "CdcMalloc.h"

#define UNUSA_PARAM(param) (void)param

#if ((defined CONF_KERNEL_VERSION_4_9) || \
     (defined CONF_KERNEL_VERSION_4_4) || \
     (defined CONF_KERNEL_VERSION_3_10))
#define KERNEL_VERSION_OLD 1
#endif

//----------------------
#define DEBUG_ION_REF 0   //just for H3 ION memery info debug

#if DEBUG_ION_REF
static int cdx_use_mem = 0;
#define ION_BUF_LEN  50

typedef struct ION_BUF_NODE_TEST
{
    unsigned long addr;
    int size;
} ion_buf_node_test;

static ion_buf_node_test ion_buf_nodes_test[ION_BUF_LEN];
#endif
//----------------------


typedef struct BUFFER_NODE
{
    struct aw_mem_list_head i_list;
    unsigned long phy;        //phisical address
    unsigned char *vir;        //virtual address
    unsigned int size;        //buffer size
    unsigned int tee;      //
    unsigned char *user_virt;//
    int dma_buf_fd;
} buffer_node;

typedef struct ION_ALLOC_CONTEXT
{
    int                        fd;         // driver handle
    struct aw_mem_list_head    list;       // buffer list
    int                        ref_cnt;    // reference count
    unsigned int               phyOffset;
    VeOpsS*                    ve_ops;
    void*                      ve_self;
    int                        inter_ve;
} ion_alloc_context;

static ion_alloc_context    *g_context = NULL;
static pthread_mutex_t       g_mutex_alloc = PTHREAD_MUTEX_INITIALIZER;


static int get_phy_addr(int nIonFd, int nBufFd, unsigned long *pAddr)
{
    struct user_iommu_param nIommuBuffer;

#if KERNEL_VERSION_OLD
    unsigned long phy = 0;
    int ret = 0;
    aw_ion_user_handle_t ion_handle;

    if(CdcIonGetMemType() == MEMORY_IOMMU)
    {
        nIommuBuffer.fd = nBufFd;
        CdcVeGetIommuAddr(g_context->ve_ops, g_context->ve_self, &nIommuBuffer);

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

#else
    UNUSA_PARAM(nIonFd);

    nIommuBuffer.fd = nBufFd;
    CdcVeGetIommuAddr(g_context->ve_ops, g_context->ve_self, &nIommuBuffer);
    if(nIommuBuffer.iommu_addr & 0xff)
    {
        loge("get iommu addr maybe wrong:%x", nIommuBuffer.iommu_addr);
        return -1;
    }

    *pAddr = (unsigned long)nIommuBuffer.iommu_addr;
#endif
    return 0;
}

/*funciton begin*/

static int ion_alloc_open2(void* veops, void* veself)
{
    pthread_mutex_lock(&g_mutex_alloc);
    if (g_context != NULL)
    {
        g_context->ref_cnt++;
        logd("ion context already create, ref_count:%d\n",g_context->ref_cnt);
        pthread_mutex_unlock(&g_mutex_alloc);
        return 0;
    }

    g_context = (ion_alloc_context*)malloc(sizeof(ion_alloc_context));
    if (g_context == NULL)
    {
        loge("create ion allocator failed, out of memory\n");
        return -1;
    }
    else
    {
        logd("pid: %d, g_context = %p\n", getpid(), g_context);
    }

    memset((void*)g_context, 0, sizeof(ion_alloc_context));

    g_context->fd = CdcIonOpen();

    if (g_context->fd <= 0)
    {
        loge("open ion failed");
        goto ERROR_OUT;
    }

    g_context->ve_ops = (VeOpsS*)veops;
    g_context->ve_self = veself;
    g_context->phyOffset = CdcVeGetPhyOffset((VeOpsS*)veops, veself);

#if DEBUG_ION_REF
    cdx_use_mem = 0;
    memset(&ion_buf_nodes_test, 0, sizeof(ion_buf_nodes_test));
    logd("ion_open, cdx_use_mem=[%dByte].", cdx_use_mem);
    CdcIonGetTotalSize(g_context->fd);
#endif

    AW_MEM_INIT_LIST_HEAD(&g_context->list);
    g_context->ref_cnt++;
    pthread_mutex_unlock(&g_mutex_alloc);
    logd("ion alloc open ok");

    return 0;

ERROR_OUT:
    free(g_context);
    g_context = NULL;

    pthread_mutex_unlock(&g_mutex_alloc);
    return -1;
}

static int ion_alloc_open()
{
    VeOpsS* veOps = NULL;
    void* pVeopsSelf = NULL;

    pthread_mutex_lock(&g_mutex_alloc);
    if (g_context != NULL)
    {
        g_context->ref_cnt++;
        logd("ion context already create, ref_count:%d\n",g_context->ref_cnt);
        pthread_mutex_unlock(&g_mutex_alloc);
        return 0;
    }
    pthread_mutex_unlock(&g_mutex_alloc);

    veOps = GetVeOpsS(VE_OPS_TYPE_NORMAL);
    if(veOps == NULL)
    {
        loge("get ve ops failed");
        return -1;
    }
    VeConfig mVeConfig;
    memset(&mVeConfig, 0, sizeof(VeConfig));
    mVeConfig.nDecoderFlag = 1;
    mVeConfig.bNotSetVeFreq = 1;

    pVeopsSelf = CdcVeInit(veOps,&mVeConfig);
    if(pVeopsSelf == NULL)
    {
        loge("init ve ops failed");
        return -1;
    }

    if(ion_alloc_open2(veOps, pVeopsSelf) != 0)
    {
        CdcVeRelease(veOps, pVeopsSelf);
        return -1;
    }

    //first open
    pthread_mutex_lock(&g_mutex_alloc);
    g_context->inter_ve = 1;
    pthread_mutex_unlock(&g_mutex_alloc);
    return 0;
}

static void ion_alloc_close()
{
    struct aw_mem_list_head * pos, *q;

    pthread_mutex_lock(&g_mutex_alloc);
    if(g_context == NULL)
    {
        loge("ref cnt error, please check!");
        pthread_mutex_unlock(&g_mutex_alloc);
        return;
    }
    else if(g_context->ref_cnt > 1)
    {
        logd("ref cnt: %d > 1, do not free\n", g_context->ref_cnt);
        g_context->ref_cnt--;
        pthread_mutex_unlock(&g_mutex_alloc);
        return;
    }

    logd("pid: %d, release g_context = %p\n", getpid(), g_context);

    aw_mem_list_for_each_safe(pos, q, &g_context->list)
    {
        buffer_node * tmp;
        tmp = aw_mem_list_entry(pos, buffer_node, i_list);
        logv("ion_alloc_close del item phy= 0x%lx vir= 0x%lx, size= %d\n",
            tmp->phy, (unsigned long)tmp->vir, tmp->size);
        aw_mem_list_del(pos);
        free(tmp);
    }

    close(g_context->fd);
    g_context->fd = 0;

    if(g_context->inter_ve)
        CdcVeRelease(g_context->ve_ops, g_context->ve_self);

    //--------------
#if DEBUG_ION_REF
    int i = 0;
    int counter = 0;
    logd("ion_close, cdx_use_mem=[%d MB]", cdx_use_mem/1024/1024);
    CdcIonGetTotalSize(g_context->fd);
    for(i=0; i<ION_BUF_LEN; i++)
    {
        if(ion_buf_nodes_test[i].addr != 0 || ion_buf_nodes_test[i].size != 0){

            loge("ion mem leak????  addr->[0x%lx], leak size->[%dByte]", \
                ion_buf_nodes_test[i].addr, ion_buf_nodes_test[i].size);
            counter ++;
        }
    }

    if(counter != 0)
    {
        loge("my god, have [%d]blocks ion mem leak.!!!!", counter);
    }
    else
    {
        logd("well done, no ion mem leak.");
    }
#endif
    //--------------
    //CdcMallocReport();
    free(g_context);
    g_context = NULL;
    logd("ion_alloc_close ok\n");
    pthread_mutex_unlock(&g_mutex_alloc);
    return ;
}

static void* ion_alloc_palloc_base(int size, void *veOps, void *pVeopsSelf, unsigned char bIsCache)
{
    unsigned int heap_mask;
    unsigned int flags = 0;
    int dma_buf_fd;
    unsigned long addr_phy = 0;
    unsigned char* addr_vir = NULL;
    buffer_node * alloc_buffer = NULL;
    int ret = 0;

    UNUSA_PARAM(veOps);
    UNUSA_PARAM(pVeopsSelf);
    pthread_mutex_lock(&g_mutex_alloc);

    if (g_context == NULL)
    {
        loge("ion_alloc do not opened");
        goto ALLOC_OUT;
    }

    if(size <= 0)
    {
        loge("can not alloc size 0 \n");
        goto ALLOC_OUT;
    }

    if(CdcIonGetMemType() == MEMORY_IOMMU)
    {
        heap_mask = AW_SYSTEM_HEAP_MASK | AW_CARVEROUT_HEAP_MASK;
    }
    else
    {
        logd("ji*****dma heap");
        heap_mask = AW_DMA_HEAP_MASK | AW_CARVEROUT_HEAP_MASK;
    }

    if(bIsCache != 0)
    {
        flags = AW_ION_CACHED_FLAG | AW_ION_CACHED_NEEDS_SYNC_FLAG;
    }
    logv("ion alloc fd:%d size:%d heap:%d, falgs:%d", g_context->fd,size,heap_mask,flags);
    ret = CdcIonAllocFd(g_context->fd, size, SZ_4k, heap_mask, flags, &dma_buf_fd);

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

    ret = get_phy_addr(g_context->fd, dma_buf_fd, &addr_phy);
    if(ret < 0)
    {
        loge("get phy addr error\n");
        CdcIonMunmap(size, addr_vir);
        goto ALLOC_OUT;
    }

    alloc_buffer = (buffer_node *)malloc(sizeof(buffer_node));
    if (alloc_buffer == NULL)
    {
        loge("malloc buffer node failed");
        addr_phy = 0;
        addr_vir = 0;        // value of MAP_FAILED is -1, should return 0
        CdcIonMunmap(size, addr_vir);
        goto ALLOC_OUT;
    }

    alloc_buffer->phy        = addr_phy;
    alloc_buffer->vir        = addr_vir;
    alloc_buffer->user_virt  = addr_vir;
    alloc_buffer->size       = size;
    alloc_buffer->dma_buf_fd = dma_buf_fd;

    logv("alloc succeed, addr_phy: 0x%lx, addr_vir: %p, size: %d", addr_phy, addr_vir, size);

    aw_mem_list_add_tail(&alloc_buffer->i_list, &g_context->list);

    //------start-----------------
#if DEBUG_ION_REF
    cdx_use_mem += size;
    logd("++++++cdx_use_mem = [%d MB], increase size->[%d B], addr_vir=[0x%lx], addr_phy=[0x%lx]", \
        cdx_use_mem/1024/1024, size, addr_vir, addr_phy);
    int i = 0;
    for(i=0; i<ION_BUF_LEN; i++)
    {
        if(ion_buf_nodes_test[i].addr == 0 && ion_buf_nodes_test[i].size == 0){
            ion_buf_nodes_test[i].addr = addr_vir;
            ion_buf_nodes_test[i].size = size;
            break;
        }
    }

    if(i>= ION_BUF_LEN){
        loge("error, ion buf len is large than [%d]", ION_BUF_LEN);
    }
#endif
//--------------------------------

ALLOC_OUT:
    pthread_mutex_unlock(&g_mutex_alloc);
    return (void*)addr_vir;
}

static void* ion_alloc_palloc(int size, void *veOps, void *pVeopsSelf)
{
    unsigned char bIsCache = 1;
    return ion_alloc_palloc_base(size, veOps, pVeopsSelf, bIsCache);
}

static void* ion_alloc_no_cache_palloc(int size, void *veOps, void *pVeopsSelf)
{
    unsigned char bIsCache = 0;
    return ion_alloc_palloc_base(size, veOps, pVeopsSelf, bIsCache);
}

static void ion_alloc_pfree(void * pbuf, void *veOps, void *pVeopsSelf)
{
    int flag = 0;
    unsigned char* addr_vir = (unsigned char*)pbuf;
    buffer_node * tmp;
    struct user_iommu_param nIommuBuffer;
    if (0 == pbuf)
    {
        loge("can not free NULL buffer\n");
        return ;
    }

    UNUSA_PARAM(veOps);
    UNUSA_PARAM(pVeopsSelf);

    pthread_mutex_lock(&g_mutex_alloc);

    if (g_context == NULL)
    {
        loge("ion_alloc do not opened, should call ion_alloc_open() \
            before ion_alloc_alloc(size) \n");
        pthread_mutex_unlock(&g_mutex_alloc);
        return ;
    }

    aw_mem_list_for_each_entry(tmp, &g_context->list, i_list)
    {
        if (tmp->vir == addr_vir)
        {
            logv("free phy= 0x%lx vir= 0x%lx, size= %d\n",tmp->phy, (unsigned long)tmp->vir, tmp->size);
            /*unmap user space*/
#if KERNEL_VERSION_OLD
            if(CdcIonGetMemType() == MEMORY_IOMMU )
            {
                logv("ion_pfree: fd:%d, iommu_addr:0x%lx\n", tmp->dma_buf_fd, tmp->phy);
                nIommuBuffer.fd = tmp->dma_buf_fd;
                nIommuBuffer.iommu_addr = tmp->phy;
                if(CdcVeFreeIommuAddr(g_context->ve_ops, g_context->ve_self, &nIommuBuffer) < 0)
                    loge("VeFreeIommuAddr error\n");
            }
#else
            nIommuBuffer.fd = tmp->dma_buf_fd;
            nIommuBuffer.iommu_addr = tmp->phy;
            logv("ion pfree: fd:%d, iommu_addr:%lx\n", tmp->dma_buf_fd, tmp->phy);

            if(CdcVeFreeIommuAddr(g_context->ve_ops,g_context->ve_self, &nIommuBuffer) < 0)
                loge("VeFreeIommuAddr error\n");
#endif

            CdcIonMunmap(tmp->size, tmp->vir);
            /*close dma buffer fd*/
            close(tmp->dma_buf_fd);
            aw_mem_list_del(&tmp->i_list);
            free(tmp);

            flag = 1;

            //------start-----------------
#if DEBUG_ION_REF
            int i = 0;
            for(i=0; i<ION_BUF_LEN; i++)
            {
                if(ion_buf_nodes_test[i].addr == addr_vir && ion_buf_nodes_test[i].size > 0){

                    cdx_use_mem -= ion_buf_nodes_test[i].size;
                    logv("--------cdx_use_mem = [%d MB], reduce size->[%d B]",\
                        cdx_use_mem/1024/1024, ion_buf_nodes_test[i].size);
                    ion_buf_nodes_test[i].addr = 0;
                    ion_buf_nodes_test[i].size = 0;

                    break;
                }
            }

            if(i>= ION_BUF_LEN){
                loge("error, ion buf len is large than [%d]", ION_BUF_LEN);
            }
#endif
            //--------------------------------

            break;
        }
    }

    if (0 == flag)
    {
        loge("ion_alloc_free failed, do not find virtual address: %p", addr_vir);
    }

    pthread_mutex_unlock(&g_mutex_alloc);
    return ;
}

//* get the total alloc buffer size
static int ion_alloc_get_debug_info(char*  pDst, int nDstBufsize)
{
    buffer_node * tmp;
    int nTotalSize = 0;
    int count = 0;

    pthread_mutex_lock(&g_mutex_alloc);

    aw_mem_list_for_each_entry(tmp, &g_context->list, i_list)
    {
        nTotalSize += tmp->size;
    }

    pthread_mutex_unlock(&g_mutex_alloc);

    count += sprintf(pDst + count, "palloc: %d MB, %d KB, %d B\n",
                     nTotalSize/1024/1024, nTotalSize/1024, nTotalSize);

    if(count > nDstBufsize)
       count = nDstBufsize;

    return count;
}

static void* ion_alloc_vir2phy_cpu(void * pbuf)
{
    int flag = 0;
    unsigned char* addr_vir = (unsigned char*)pbuf;
    unsigned long addr_phy = 0;
    buffer_node * tmp;

    if (0 == pbuf)
    {
        // logv("can not vir2phy NULL buffer \n");
        return 0;
    }

    pthread_mutex_lock(&g_mutex_alloc);

    aw_mem_list_for_each_entry(tmp, &g_context->list, i_list)
    {
        if (addr_vir >= tmp->vir
            && addr_vir < tmp->vir + tmp->size)
        {
            addr_phy = tmp->phy + (addr_vir - tmp->vir);
            // logv("ion_alloc_vir2phy phy= 0x%08x vir= 0x%08x\n", addr_phy, addr_vir);
            flag = 1;
            break;
        }
    }

    if (0 == flag)
    {
        loge("ion_alloc_vir2phy failed, do not find virtual address: %p", addr_vir);
    }

    pthread_mutex_unlock(&g_mutex_alloc);

    return (void*)addr_phy;
}

static void* ion_alloc_phy2vir_cpu(void * pbuf)
{
    int flag = 0;
    unsigned char* addr_vir = NULL;
    unsigned long addr_phy = (unsigned long)pbuf;
    buffer_node * tmp;

    if (NULL == pbuf)
    {
        loge("can not phy2vir NULL buffer\n");
        return NULL;
    }

    pthread_mutex_lock(&g_mutex_alloc);

    aw_mem_list_for_each_entry(tmp, &g_context->list, i_list)
    {
        if (addr_phy >= tmp->phy && addr_phy < tmp->phy + tmp->size)
        {
            addr_vir = tmp->vir + (addr_phy - tmp->phy);
            flag = 1;
            break;
        }
    }

    if (0 == flag)
    {
        loge("ion_alloc_phy2vir failed, do not find physical address: 0x%lx \n", addr_phy);
    }

    pthread_mutex_unlock(&g_mutex_alloc);

    return (void*)addr_vir;
}

static void* ion_alloc_vir2phy_ve(void * pbuf)
{
    logv("**11 phy offset = %x",g_context->phyOffset);

    return (void*)((unsigned long)ion_alloc_vir2phy_cpu(pbuf) - g_context->phyOffset);
}

static void* ion_alloc_phy2vir_ve(void * pbuf)
{
    logv("**22 phy offset = %x",g_context->phyOffset);

    return (void*)((unsigned long)ion_alloc_phy2vir_cpu(pbuf) - g_context->phyOffset);
}

static void ion_alloc_flush_cache(void* startAddr, int size)
{
#if KERNEL_VERSION_OLD
    CdcIonFlushCache(g_context->fd, 0, startAddr, size);
#else
    CdcVeFlushCache(g_context->ve_ops, g_context->ve_self, startAddr, size);
#endif
}

static int ion_alloc_get_total_size()
{
    if(g_context == NULL)
    {
        loge("context is null");
        return -1;
    }
    return CdcIonGetTotalSize(g_context->fd);
}

static void* ion_alloc_get_vir(int dmafd)
{
    int flag = 0;
    unsigned char* addr_vir = NULL;
    buffer_node * tmp = NULL;

    pthread_mutex_lock(&g_mutex_alloc);

    aw_mem_list_for_each_entry(tmp, &g_context->list, i_list)
    {
        if (dmafd == tmp->dma_buf_fd)
        {
            addr_vir = tmp->vir;
            flag = 1;
            break;
        }
    }

    if (0 == flag)
    {
        loge("ion_alloc_get_vir failed, do not find viraddr by dmafd %d\n", dmafd);
    }

    pthread_mutex_unlock(&g_mutex_alloc);

    return (void*)addr_vir;
}

static int ion_alloc_get_phy(int dmafd, void* phyaddr)
{
    struct user_iommu_param nIommuBuffer;
    unsigned int* tmp_phy = (unsigned int*)phyaddr;
    if(g_context == NULL)
    {
        loge("context is null");
        return -1;
    }

    nIommuBuffer.fd = dmafd;
    CdcVeGetIommuAddr(g_context->ve_ops, g_context->ve_self, &nIommuBuffer);
    if(nIommuBuffer.iommu_addr & 0xff)
    {
        loge("get iommu addr maybe wrong:%x", nIommuBuffer.iommu_addr);
        return -1;
    }

    *tmp_phy = nIommuBuffer.iommu_addr;

    return 0;
}

static int ion_alloc_free_phy(int dmafd, unsigned long phy)
{
    struct user_iommu_param nIommuBuffer;

    if(g_context == NULL)
    {
        loge("context is null");
        return -1;
    }
    nIommuBuffer.fd = dmafd;
    nIommuBuffer.iommu_addr = phy;
    logv("ion pfree: fd:%d, iommu_addr:%lx\n", dmafd, phy);

    if(CdcVeFreeIommuAddr(g_context->ve_ops,g_context->ve_self, &nIommuBuffer) < 0)
    {
        loge("VeFreeIommuAddr error\n");
        return -1;
    }
    return 0;
}

static int ion_alloc_memset(void* buf, int value, size_t n)
{
    memset(buf, value, n);
    return -1;
}

static int ion_alloc_copy(void* dst, void* src, size_t n)
{
    memcpy(dst, src, n);
    return -1;
}

static int ion_alloc_read(void* dst, void* src, size_t n)
{
    memcpy(dst, src, n);
    return -1;
}

static int ion_alloc_write(void* dst, void* src, size_t n)
{
    memcpy(dst, src, n);
    return -1;
}

static int ion_alloc_setup()
{
    return -1;
}

static int ion_alloc_shutdown()
{
    return -1;
}

static unsigned int ion_alloc_get_ve_addr_offset()
{
    if(g_context != NULL)
        return g_context->phyOffset;
    else
    {
        loge("g_context is NULL, please call ion_alloc_open\n");
        return 0;
    }
}

static int ion_alloc_get_fd(void* pbuf)
{
    int flag = 0;
    unsigned char* addr_vir = (unsigned char*)pbuf;
    int buffer_fd = 0;
    buffer_node * tmp;

    if (0 == pbuf) {
        loge("can not get fd!\n");
        return 0;
    }
    pthread_mutex_lock(&g_mutex_alloc);
    aw_mem_list_for_each_entry(tmp, &g_context->list, i_list)
    {
        if (addr_vir >= tmp->vir
            && addr_vir < tmp->vir + tmp->size) {

            buffer_fd = tmp->dma_buf_fd;
            logv("ion_alloc_get_fd, fd = 0x%08x vir= 0x%08x\n", buffer_fd, (unsigned int)addr_vir);
            flag = 1;
            break;
        }
    }
    if (0 == flag)
        loge("ion_alloc_get_fd failed,no virtual address: %p\n", addr_vir);
    pthread_mutex_unlock(&g_mutex_alloc);

    logv("*** get_bufferfd: %d, flag = %d\n",buffer_fd, flag);
    return buffer_fd;
}

static struct ScMemOpsS _ionMemOpsS =
{
    .open  =              ion_alloc_open,
    .open2 =              ion_alloc_open2,
    .close =              ion_alloc_close,
    .total_size=          ion_alloc_get_total_size,
    .palloc =             ion_alloc_palloc,
    .palloc_no_cache =    ion_alloc_no_cache_palloc,
    .pfree =              ion_alloc_pfree,
    .flush_cache =        ion_alloc_flush_cache,
    .ve_get_phyaddr=      ion_alloc_vir2phy_ve,
    .ve_get_viraddr =     ion_alloc_phy2vir_ve,
    .cpu_get_phyaddr =    ion_alloc_vir2phy_cpu,
    .cpu_get_viraddr =    ion_alloc_phy2vir_cpu,
    .mem_set =            ion_alloc_memset,
    .mem_cpy =            ion_alloc_copy,
    .mem_read =           ion_alloc_read,
    .mem_write =          ion_alloc_write,
    .setup =              ion_alloc_setup,
    .shutdown =           ion_alloc_shutdown,
    .get_ve_addr_offset = ion_alloc_get_ve_addr_offset,
    .get_debug_info =     ion_alloc_get_debug_info,
    .get_vir_by_fd =      ion_alloc_get_vir,
    .get_phy_by_fd =      ion_alloc_get_phy,
    .free_phy_by_fd =     ion_alloc_free_phy,
    .get_fd_by_vir  =     ion_alloc_get_fd
};

struct ScMemOpsS* __GetIonMemOpsS()
{
    logd("*** get __GetIonMemOpsS ***");
    return &_ionMemOpsS;
}
