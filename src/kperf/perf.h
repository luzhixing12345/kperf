
#pragma once

#include <stdint.h>

#define SAMPLE_CAPACITY        4000
#define SAMPLE_GROWTH_FACTOR   2
#define SAMPLE_CALLCHAIN_DEPTH 8

struct perf_sample {
    uint32_t pid;
    uint32_t tid;
    uint64_t nr;
    uint64_t *ips;
};

struct perf_sample_table {
    struct perf_sample *samples;
    int size;
    int capacity;
};

int init_perf_sample_table(struct perf_sample_table *table);
void free_perf_sample_table(struct perf_sample_table *table);
void add_perf_sample(struct perf_sample_table *table, uint32_t pid, uint32_t tid, uint64_t nr, uint64_t *ips);
int profile_process(int cgroup_fd, int sample_freq);