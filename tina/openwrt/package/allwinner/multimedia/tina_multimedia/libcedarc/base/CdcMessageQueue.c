/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : messageQueue.c
 * Description : message queue
 * History :
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <malloc.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>

#include "cdc_log.h"
#include "CdcMessageQueue.h"

//* should include other  '*.h'  before CdcMalloc.h
#include "CdcMalloc.h"


typedef struct MessageNode MessageNode;

struct MessageNode
{
    MessageNode* next;
    int          valid;
    CdcMessage    *msg;
};

typedef struct MessageQueueContext
{
    char*           pName;
    MessageNode*    pHead;
    int             nCount;
    MessageNode*    Nodes;
    int             nMaxMessageNum;
    size_t          nMessageSize;
    pthread_mutex_t mutex;
    sem_t           sem;
}MessageQueueContext;

CdcMessageQueue* CdcMessageQueueCreate(int nMaxMessageNum, const char* pName)
{
    MessageQueueContext* mqCtx;
    size_t nMessageSize = sizeof(CdcMessage);
    logd("nMessageSize = %d",(int)nMessageSize);

    mqCtx = (MessageQueueContext*)malloc(sizeof(MessageQueueContext));
    if(mqCtx == NULL)
    {
        loge("%s, allocate memory fail.", pName);
        return NULL;
    }
    memset(mqCtx, 0, sizeof(MessageQueueContext));

    if(pName != NULL)
        mqCtx->pName = strdup(pName);

    mqCtx->Nodes = (MessageNode*)calloc(nMaxMessageNum, sizeof(MessageNode));
    if(mqCtx->Nodes == NULL)
    {
        loge("%s, allocate memory for message nodes fail.", mqCtx->pName);
        if(mqCtx->pName != NULL)
            free(mqCtx->pName);
        free(mqCtx);
        return NULL;
    }

    int i;
    for (i = 0; i < nMaxMessageNum; i++)
    {
        mqCtx->Nodes[i].msg = calloc(1, nMessageSize);
        if (mqCtx->Nodes[i].msg == NULL)
        {
            int j;
            for (j = 0; j < i; j++)
                free(mqCtx->Nodes[j].msg);
            free(mqCtx->pName);
            free(mqCtx->Nodes);
            free(mqCtx);
            return NULL;
        }
    }

    mqCtx->nMaxMessageNum = nMaxMessageNum;
    mqCtx->nMessageSize = nMessageSize;

    pthread_mutex_init(&mqCtx->mutex, NULL);
    sem_init(&mqCtx->sem, 0, 0);

    return (CdcMessageQueue*)mqCtx;
}

void CdcMessageQueueDestroy(CdcMessageQueue* mq)
{
    MessageQueueContext* mqCtx;

    mqCtx = (MessageQueueContext*)mq;

    int i;
    for (i = 0; i < mqCtx->nMaxMessageNum; i++)
        free(mqCtx->Nodes[i].msg);

    if(mqCtx->Nodes != NULL)
    {
        free(mqCtx->Nodes);
    }

    pthread_mutex_destroy(&mqCtx->mutex);
    sem_destroy(&mqCtx->sem);

    if(mqCtx->pName != NULL)
        free(mqCtx->pName);

    free(mqCtx);

    return;
}

int CdcMessageQueuePostMessage(CdcMessageQueue* mq, CdcMessage* m)
{
    MessageQueueContext* mqCtx;
    MessageNode*         node;
    MessageNode*         ptr;
    int                  i;

    mqCtx = (MessageQueueContext*)mq;

    pthread_mutex_lock(&mqCtx->mutex);

    if(mqCtx->nCount >= mqCtx->nMaxMessageNum)
    {
        loge("%s, message count exceed, current message count = %d, max message count = %d",
                mqCtx->pName, mqCtx->nCount, mqCtx->nMaxMessageNum);
        pthread_mutex_unlock(&mqCtx->mutex);
        return -1;
    }

    node = NULL;
    ptr  = mqCtx->Nodes;
    for(i=0; i<mqCtx->nMaxMessageNum; i++, ptr++)
    {
        if(ptr->valid == 0)
        {
            node = ptr;
            break;
        }
    }

    if(!node){
        loge("%s, node is NULL", mqCtx->pName);
        pthread_mutex_unlock(&mqCtx->mutex);
        return -1;
    }

    memcpy(node->msg, m, mqCtx->nMessageSize);
    node->valid         = 1;
    node->next          = NULL;

    ptr = mqCtx->pHead;
    if(ptr == NULL)
        mqCtx->pHead = node;
    else
    {
        while(ptr->next != NULL)
            ptr = ptr->next;

        ptr->next = node;
    }

    mqCtx->nCount++;

    pthread_mutex_unlock(&mqCtx->mutex);

    sem_post(&mqCtx->sem);

    return 0;
}

int CdcMessageQueueWaitMessage(CdcMessageQueue* mq, int64_t timeout)
{
    CEDARC_UNUSE(CdcMessageQueueWaitMessage);
    if (SemTimedWait(&mq->sem, timeout) < 0)
        return -1;

    sem_post(&mq->sem);
    return mq->nCount;
}

int CdcMessageQueueGetMessage(CdcMessageQueue* mq, CdcMessage* m)
{
    CEDARC_UNUSE(CdcMessageQueueGetMessage);
    return CdcMessageQueueTryGetMessage(mq, m, -1);
}

int CdcMessageQueueTryGetMessage(CdcMessageQueue* mq, CdcMessage* m, int64_t timeout)
{
    MessageQueueContext* mqCtx;
    MessageNode*         node;

    mqCtx = (MessageQueueContext*)mq;

    if(SemTimedWait(&mqCtx->sem, timeout) < 0)
    {
        return -1;
    }

    pthread_mutex_lock(&mqCtx->mutex);

    if(mqCtx->nCount <= 0)
    {
        logv("%s, no message.", mqCtx->pName);
        pthread_mutex_unlock(&mqCtx->mutex);
        return -1;
    }

    node = mqCtx->pHead;
    mqCtx->pHead = node->next;

    memcpy(m, node->msg, mqCtx->nMessageSize);
    node->valid = 0;

    mqCtx->nCount--;

    pthread_mutex_unlock(&mqCtx->mutex);

    return 0;
}

int CdcMessageQueueFlush(CdcMessageQueue* mq)
{
    CEDARC_UNUSE(CdcMessageQueueFlush);
    MessageQueueContext* mqCtx;
    int                  i;

    mqCtx = (MessageQueueContext*)mq;

    logi("%s, flush messages.", mqCtx->pName);

    pthread_mutex_lock(&mqCtx->mutex);

    mqCtx->pHead  = NULL;
    mqCtx->nCount = 0;
    for(i=0; i<mqCtx->nMaxMessageNum; i++)
    {
        mqCtx->Nodes[i].valid = 0;
    }

    do
    {
        if(sem_getvalue(&mqCtx->sem, &i) != 0 || i == 0)
            break;

        sem_trywait(&mqCtx->sem);

    } while(1);

    pthread_mutex_unlock(&mqCtx->mutex);

    return 0;
}

int CdcMessageQueueGetCount(CdcMessageQueue* mq)
{
    CEDARC_UNUSE(CdcMessageQueueGetCount);
    MessageQueueContext* mqCtx;

    mqCtx = (MessageQueueContext*)mq;

    return mqCtx->nCount;
}

#include <errno.h>
#define min(x, y)   ((x) <= (y) ? (x) : (y));

static int64_t CdcMonoTimeUs(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        abort();

    return (1000000LL * ts.tv_sec) + (ts.tv_nsec / 1000);
}

int SemTimedWait(sem_t* sem, int64_t time_ms)
{
    int err;

    if(time_ms == -1)
    {
        err = sem_wait(sem);
    }
    else
    {
        #if 0
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += time_ms % 1000 * 1000 * 1000;
        ts.tv_sec += time_ms / 1000 + ts.tv_nsec / (1000 * 1000 * 1000);
        ts.tv_nsec = ts.tv_nsec % (1000*1000*1000);

        err = sem_timedwait(sem, &ts);
        #else
        // sem_trywait + usleep的方式实现
        // 如果信号量大于0，则减少信号量并立马返回true
        // 如果信号量小于0，则阻塞等待，当阻塞超时时返回false
        const int64_t timeoutUs = time_ms * 1000; // 延时时间由毫米转换为微秒
        const int64_t maxTimeWait = 100000; // 最大的睡眠的时间为100000微秒，也就是100毫秒

        int64_t timeWait = 10; // 睡眠时间，默认为10微秒
        int64_t delayUs = 0; // 剩余需要延时睡眠时间

        const int64_t startUs = CdcMonoTimeUs(); // 循环前的开始时间，单位微秒
        int64_t elapsedUs = 0; // 过期时间，单位微秒

        int ret = 0;

        do
        {
            // 如果信号量大于0，则减少信号量并立马返回true
            if(sem_trywait(sem) == 0)
            {
                return 0;
            }

            // 系统信号则立马返回false
            if(errno != EAGAIN)
            {
                return -1;
            }

            // delayUs一定是大于等于0的，因为do-while的条件是elapsedUs <= timeoutUs.
            delayUs = timeoutUs - elapsedUs;

            // 睡眠时间取最小的值
            timeWait = min(delayUs, timeWait);

            // 进行睡眠 单位是微秒
            ret = usleep(timeWait);
            if( ret != 0 )
            {
                return -1;
            }

            // 睡眠延时时间双倍自增
            timeWait *= 2;

            // 睡眠延时时间不能超过最大值
            timeWait = min(timeWait, maxTimeWait);

            // 计算开始时间到现在的运行时间 单位是微秒
            elapsedUs = CdcMonoTimeUs() - startUs;
        } while( elapsedUs <= timeoutUs ); // 如果当前循环的时间超过预设延时时间则退出循环

        return -1;
        #endif
    }

    return err;
}

