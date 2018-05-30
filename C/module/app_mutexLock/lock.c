#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define     PTHREAD_NUM     (pthread_self() % 1000)

pthread_mutex_t mutex_1;
pthread_mutex_t mutex_2;

static unsigned int count_1;
static unsigned int count_2;

static void *pthread_1(void *argc)
{
    int res_1 = 0, res_2 = 0, res_3 = 0;
    printf("[%03d\t%04lu], pthread_1 start runging!\n", __LINE__, PTHREAD_NUM);
    while (1) {
        res_1 = pthread_mutex_lock(&mutex_1);
        count_1++;
        res_2 = pthread_mutex_lock(&mutex_2);
        count_2++;
        printf("[%03d\t%04lu], pthread_1, count_1[%d,] count_2[%d], res_1[%d], res_2[%d], res_3[%d]\n", __LINE__, PTHREAD_NUM, count_1, count_2, res_1, res_2, res_3);
        pthread_mutex_unlock(&mutex_2);
        pthread_mutex_unlock(&mutex_1);
        //usleep(10);
    }
}

static void *pthread_2(void *argc)
{
    int res_1 = 0, res_2 = 0;
    printf("[%03d\t%04lu], pthread_2 start runging!\n", __LINE__, PTHREAD_NUM);
    while (1) {
        res_1 = pthread_mutex_lock(&mutex_2);
        count_1++;
        res_2 = pthread_mutex_lock(&mutex_1);
        count_2++;
        printf("[%03d\t%04lu], pthread_2, count_1[%d,] count_2[%d], res_1[%d], res_2[%d]\n", __LINE__, PTHREAD_NUM, count_1, count_2, res_1, res_2);
        pthread_mutex_unlock(&mutex_1);
        pthread_mutex_unlock(&mutex_2);
        //usleep(10);
    }
}

int main()
{
    printf("[%03d\t%04lu], main function start!\n", __LINE__, PTHREAD_NUM);
    pthread_t tid1, tid2;
    pthread_mutex_init(&mutex_1, NULL);
    pthread_mutex_init(&mutex_2, NULL);
    pthread_create(&tid1, NULL, pthread_1, NULL);
    pthread_create(&tid2, NULL, pthread_2, NULL);
    printf("[%03d\t%04lu], pthread_1 and pthread_2 create success!\n", __LINE__, PTHREAD_NUM);
    while (1) {
		;
    }
    return 0;
}
