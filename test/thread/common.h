
#include <time.h>

#define MAX_RUNTIME 5

static void busy_no_work(void)
{
    volatile unsigned long i = 0;
    for (i = 0; i < 1000000UL; i++) {
        /* prevent optimization */
        asm volatile("" ::: "memory");
    }
}

static void another_work() {
    busy_no_work();
}

static void busy_work(void)
{
    volatile unsigned long i = 0;
    for (i = 0; i < 100000000UL; i++) {
        /* prevent optimization */
        asm volatile("" ::: "memory");
    }
    another_work();
}


static void run_5s_loop(void)
{
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        busy_work();
        busy_no_work();
        clock_gettime(CLOCK_MONOTONIC, &now);
        if ((now.tv_sec - start.tv_sec) >= MAX_RUNTIME)
            break;
    }
}
