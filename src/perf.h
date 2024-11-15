

#pragma once

#include <asm/unistd_64.h>
#include <linux/perf_event.h>
#include <sys/types.h>
#include <unistd.h>

//------------------------------perf profiler-------------------------
static long perf_event_open(struct perf_event_attr *perf_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, perf_event, pid, cpu, group_fd, flags);
}