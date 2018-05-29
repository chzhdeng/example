#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "logger.h"
#include "queue.h"
#include "unitiy.h"

#define PMS_STACK_SIZE  (1024*100)
int count =0;
int g_running_task = 0;
Queue request_pms_queue = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, 0};

void *pmsClient(void);

void pmsClientStart()
{
    pthread_t pms_client_phread;
    pthread_attr_t pms_request_monitor;
    pthread_attr_init(&pms_request_monitor);
    pthread_attr_setstacksize((&pms_request_monitor), PMS_STACK_SIZE);
    pthread_attr_setdetachstate((&pms_request_monitor), PTHREAD_CREATE_DETACHED); //PTHREAD_CREATE_DETACHED
    pthread_create(&pms_client_phread, &pms_request_monitor, (void *)pmsClient, NULL);

}

PMSTask *createTask(void)
{
    PMSTask *task = (PMSTask *)malloc(sizeof(PMSTask));
    if (task == NULL)
    {
        return NULL;
    }
    memset(task, 0, sizeof(PMSTask));
    task->status = 0;

    return task;
}

int main()
{
	g_running_task = 1;
	pmsClientStart();
	printf("===========start======\n");
	while(g_running_task)
	{
		sleep(1);
	}
	sleep(1);
}

void *pmsClient(void)
{
	int i = 0;
	void *tmp_deq_task = NULL;
	PMSTask *task = NULL;
    PMSTask *deq_task = NULL;
    DEQUEUE_RESULT ret = DEQUEUE_FAILED;

    printf("PMS Clinet start.\n");

    queueInitialize(&request_pms_queue);

    while (g_running_task)
    {
		if (1)
		{
			for (i = 0; i < 5; i++ )
			{
				task = NULL;
				task = createTask();
				enqueueTask(&request_pms_queue, (void *)task);
			}
			count ++;
		}

        while(DEQUEUE_SUCCESS == (ret = dequeueTask(&request_pms_queue, &tmp_deq_task)))
        {
			deq_task = (PMSTask *) tmp_deq_task;
            if (deq_task)
            {
				printf("-----------handle task---------\n");
				free(deq_task);
				deq_task = NULL;
            }
            else
            {
                printf("the task is empty !\n");
                break;
            }
        }

		if (count == 10)
		{
			g_running_task = 0;
		}

        sleep(1);
        printf("test threads=============>\n");
    }

    cleanQueueForPMS(&request_pms_queue);
    printf("end client sent message !\n");

    return NULL;
}
