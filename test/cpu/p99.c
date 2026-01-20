/* Converted to C: use clock_gettime for timing and dynamic array for timings */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

#define MAX_RUNTIME 8

/* Timer helpers */
struct timer {
    struct timespec ts;
};

static void timer_start(struct timer *t) {
    clock_gettime(CLOCK_MONOTONIC, &t->ts);
}

static double timer_elapsed_ms(const struct timer *t) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double sec = (double)(now.tv_sec - t->ts.tv_sec);
    double nsec = (double)(now.tv_nsec - t->ts.tv_nsec);
    return sec * 1000.0 + nsec / 1e6;
}

void someFunction(int iteration) {
    /* 模拟一个通常执行得很快，但每 100 次迭代中的最后一次耗时较长的函数 */
    volatile double result = 0.0;
    long iterations = (iteration % 100 == 99) ? 10000000L : 100000L;

    for (long i = 0; i < iterations; ++i) {
        double v = (double)i;
        result += sqrt(atan(v));
    }
    (void)result;
}

static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db)
        return -1;
    if (da > db)
        return 1;
    return 0;
}

int main(void) {
    size_t cap = 1024;
    size_t len = 0;
    double *timings = malloc(sizeof(double) * cap);
    if (!timings)
        return 1;

    int iteration = 0;
    struct timer overall_timer;
    struct timer program_timer;
    timer_start(&overall_timer);
    timer_start(&program_timer);

    for (;;) {
        if (timer_elapsed_ms(&program_timer) >= (MAX_RUNTIME * 1000)) {
            break;
        }
        struct timer t;
        timer_start(&t);
        someFunction(iteration);
        double elapsed = timer_elapsed_ms(&t);

        if (len + 1 > cap) {
            cap *= 2;
            double *nptr = realloc(timings, sizeof(double) * cap);
            if (!nptr)
                break;
            timings = nptr;
        }
        timings[len++] = elapsed;
        iteration++;

        if (timer_elapsed_ms(&overall_timer) >= 1000.0) {
            /* compute average */
            double sum = 0.0;
            for (size_t i = 0; i < len; ++i) sum += timings[i];
            double average = len ? sum / (double)len : 0.0;

            /* compute p99 */
            qsort(timings, len, sizeof(double), cmp_double);
            size_t idx = (size_t)(len * 0.99);
            if (idx >= len)
                idx = len ? len - 1 : 0;
            double p99 = len ? timings[idx] : 0.0;

            printf("Average execution time: %.6f ms\n", average);
            printf("P99 execution time: %.6f ms\n", p99);

            /* reset */
            len = 0;
            timer_start(&overall_timer);
        }
    }

    free(timings);
    return 0;
}
