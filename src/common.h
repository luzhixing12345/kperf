

#pragma once

#include <stdio.h>

#define kprintf(fmt, ...) printf("[kperf]: " fmt, ##__VA_ARGS__)
#define eprintf(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

#define MAXN 1024
#define MAXCPU 1024
#define error(msg)   \
    do {             \
        perror(msg); \
        exit(1);     \
    } while (0)

#define KPERF_CGROUP_NAME "kperf"