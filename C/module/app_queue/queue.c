#include "queue.h"
#include <stdlib.h>
#include "unitiy.h"

void queueInitialize(Queue *queue)
{
    queue->head = queue->end = NULL;
    queue->count_q_node = 0;

    pthread_mutex_init(&queue->q_lock, NULL);
}

/***Hotel Mamager Task enqueue***/
ENQUEUE_RESULT enqueueTask(Queue *q, void *task)
{
    QueueNode* node = (QueueNode *)malloc(sizeof(QueueNode));
    if (node != NULL)
    {
        node->value = task;
        node->next = NULL;
    }
    else
    {
        return ENQUEUE_FAILED;
    }

    pthread_mutex_lock(&q->q_lock);
    if (q->head == q->end && q->end == NULL)
    {
        q->head = q->end = node;
        q->count_q_node = 1;
    }
    else if (q->head == q->end && q->end != NULL)
    {
        q->end->next = node;
        q->end = node;
        q->count_q_node = 2;
    }
    else
    {
        q->end->next = node;
        q->end = node;
        q->count_q_node = q->count_q_node + 1;
    }

    pthread_mutex_unlock(&q->q_lock);

    return ENQUEUE_SUCCESS;
}

DEQUEUE_RESULT dequeueTask(Queue *q, void **pvalue)
{
    DEQUEUE_RESULT ret = DEQUEUE_FAILED;
    QueueNode *enq_node = NULL;

    pthread_mutex_lock(&q->q_lock);
    if (0 == q->count_q_node)
    {
        pthread_mutex_unlock(&q->q_lock);
        return ret;
    }
    else if (1 == q->count_q_node)
    {
        enq_node = q->head;
        *pvalue = enq_node->value;
        q->head = q->end = NULL;
    }
    else
    {
        enq_node = q->head;
        *pvalue = enq_node->value;
        q->head = q->head->next;
    }

    if (enq_node)
    {
        free(enq_node);
        enq_node = NULL;
    }
    q->count_q_node = q->count_q_node - 1;
    pthread_mutex_unlock(&q->q_lock);
    ret = DEQUEUE_SUCCESS;

    return ret;
}

void cleanQueueForPMS(Queue *queue)
{
    QueueNode *free_q_head_node = NULL;
    PMSTask *free_task = NULL;

	pthread_mutex_lock(&queue->q_lock);
    if (queue)
    {
        while(queue->head != NULL)
        {
            free_q_head_node = queue->head->next;
            if (queue->head->value != NULL)
            {
                free_task = (PMSTask *)(queue->head->value);
                free(free_task);
                queue->head->value = NULL;
            }

            free(queue->head);
            queue->head = free_q_head_node;
        }
    }
    queue->head = queue->end = NULL;
    queue->count_q_node = 0;
    pthread_mutex_unlock(&queue->q_lock);
}

