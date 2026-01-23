#include <stdio.h>
#include <time.h>

// 测试参数宏定义
#define FOO1_ITERATIONS 1000000000L   // foo1函数循环次数
#define FOO2_ITERATIONS (2 * FOO1_ITERATIONS)   // foo2函数循环次数  
#define FOO3_ITERATIONS (3 * FOO1_ITERATIONS)   // foo3函数循环次数

#define NSEC_PER_SEC 1000000000L

static inline double timespec_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + 
           (end.tv_nsec - start.tv_nsec) / (double)NSEC_PER_SEC;
}

void foo1(int x, char *y) {
    volatile long sum = 0;
    for (long i = 0; i < FOO1_ITERATIONS; ++i) sum += x;
}

int foo2(int x, int y) {
    volatile long sum = 0;
    for (long i = 0; i < FOO2_ITERATIONS; ++i) sum += i;
    return sum;
}

void foo3() {
    volatile long sum = 0;
    for (long i = 0; i < FOO3_ITERATIONS; ++i) sum += i;
}

int main() {
    struct timespec t1, t2;
    double t_foo1, t_foo2, t_foo3, total;

    printf("=== Function Performance Test ===\n");
    printf("Configuration:\n");
    printf("  foo1 iterations: %ld\n", FOO1_ITERATIONS);
    printf("  foo2 iterations: %ld\n", FOO2_ITERATIONS);
    printf("  foo3 iterations: %ld\n", FOO3_ITERATIONS);
    printf("==================================\n\n");

    printf("Running tests...\n");
    
    printf("Executing foo1...\n");
    clock_gettime(CLOCK_MONOTONIC, &t1);
    foo1(10, NULL);
    clock_gettime(CLOCK_MONOTONIC, &t2);
    t_foo1 = timespec_diff(t1, t2);

    printf("Executing foo2...\n");
    clock_gettime(CLOCK_MONOTONIC, &t1);
    foo2(10, 20);
    clock_gettime(CLOCK_MONOTONIC, &t2);
    t_foo2 = timespec_diff(t1, t2);

    printf("Executing foo3...\n");
    clock_gettime(CLOCK_MONOTONIC, &t1);
    foo3();
    clock_gettime(CLOCK_MONOTONIC, &t2);
    t_foo3 = timespec_diff(t1, t2);

    total = t_foo1 + t_foo2 + t_foo3;

    printf("\n=== Results ===\n");
    printf("Execution time (seconds):\n");
    printf("  foo1: %.6f (%.1f%%) - %ld iterations\n", t_foo1, t_foo1 / total * 100, FOO1_ITERATIONS);
    printf("  foo2: %.6f (%.1f%%) - %ld iterations\n", t_foo2, t_foo2 / total * 100, FOO2_ITERATIONS);
    printf("  foo3: %.6f (%.1f%%) - %ld iterations\n", t_foo3, t_foo3 / total * 100, FOO3_ITERATIONS);
    printf("  Total: %.6f seconds\n", total);

    return 0;
}
