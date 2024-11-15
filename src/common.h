

#pragma once

#define kprintf(fmt, ...) printf("[kperf]: " fmt, ##__VA_ARGS__)


#define MAXN 1024
#define MAXCPU 256
#define error(msg)   \
    do {             \
        perror(msg); \
        exit(1);     \
    } while (0)
