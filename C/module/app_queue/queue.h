/****************************************************************************
 *
 * FILENAME:        $RCSfile: queue.h,v $
 *
 * LAST REVISION:   $Revision: 1.0 $
 * LAST MODIFIED:   $Date: 2016/06/27 17:08:55 $
 *
 * DESCRIPTION:
 *
 * vi: set ts=4:
 *
 * Copyright (c) 2012-2012 by Grandstream Networks, Inc.
 * All rights reserved.
 *
 * This material is proprietary to Grandstream Networks, Inc. and,
 * in addition to the above mentioned Copyright, may be
 * subject to protection under other intellectual property
 * regimes, including patents, trade secrets, designs and/or
 * trademarks.
 *
 * Any use of this material for any purpose, except with an
 * express license from Grandstream Networks, Inc. is strictly
 * prohibited.
 *
 ***************************************************************************/

#ifndef __QUEUE_H__
#define __QUEUE_H__
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>

typedef pthread_mutex_t Lock;

typedef struct node_t {
    void *value;
    struct node_t *next;
} QueueNode;

typedef struct queue_t {
    QueueNode *head;
    QueueNode *end;
    Lock q_lock;
    int count_q_node;
} Queue;

typedef enum _DEQUEUE_RESULT {
    DEQUEUE_SUCCESS,
    DEQUEUE_FAILED
} DEQUEUE_RESULT;

typedef enum _ENQUEUE_RESULT {
    ENQUEUE_SUCCESS,
    ENQUEUE_FAILED
} ENQUEUE_RESULT;

void queueInitialize(Queue *queue);
void cleanQueueForPMS(Queue *queue);
ENQUEUE_RESULT enqueueTask(Queue *q, void *task);
DEQUEUE_RESULT dequeueTask(Queue *q, void **pvalue);

