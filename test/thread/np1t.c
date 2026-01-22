#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include "common.h"

#define NPROC 4

int main(void)
{
    printf("multi process, single thread\n");

    for (int i = 0; i < NPROC; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            printf("child %d start\n", getpid());
            run_5s_loop();
            _exit(0);
        }
    }

    for (int i = 0; i < NPROC; i++)
        wait(NULL);

    return 0;
}
