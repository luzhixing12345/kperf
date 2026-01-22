
#include "common.h"
#include <stdio.h>
#include <pthread.h>

#define NTHREAD 4

static void *thread_fn(void *arg)
{
    long id = (long)arg;
    printf("thread %ld start\n", id);
    run_5s_loop();
    return NULL;
}

int main(void)
{
    pthread_t th[NTHREAD];

    printf("single process, multi thread\n");

    for (long i = 0; i < NTHREAD; i++)
        pthread_create(&th[i], NULL, thread_fn, (void *)i);

    for (int i = 0; i < NTHREAD; i++)
        pthread_join(th[i], NULL);

    return 0;
}
