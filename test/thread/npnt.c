#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include "common.h"

#define NPROC   2
#define NTHREAD 3

static void *thread_fn(void *arg)
{
    long id = (long)arg;
    printf("  thread %ld in pid %d\n", id, getpid());
    run_5s_loop();
    return NULL;
}

int main(void)
{
    printf("multi process, multi thread\n");

    for (int i = 0; i < NPROC; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            pthread_t th[NTHREAD];

            for (long t = 0; t < NTHREAD; t++)
                pthread_create(&th[t], NULL, thread_fn, (void *)t);

            for (int t = 0; t < NTHREAD; t++)
                pthread_join(th[t], NULL);

            _exit(0);
        }
    }

    for (int i = 0; i < NPROC; i++)
        wait(NULL);

    return 0;
}
