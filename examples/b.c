/*
 ============================================================================
 Name        : branch_mispred.c
 Author      : John Demme
 Version     : Mar 21, 2011
 Description : A template for perf_event. Requires Linux 2.6.32 or higher
 ============================================================================
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>

// perf counter syscall
int perf_event_open(struct perf_event_attr* hw, pid_t pid, int cpu, int grp, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw, pid, cpu, grp, flags);
}

// Setup info for perf_event
struct perf_event_attr attr[4];

// function implementation
int main(int argc, char* argv[]) {
    size_t i, y;
    uint64_t val1[4], val2[4];
    int fd[4], rc;

    attr[0].type = PERF_TYPE_HARDWARE;
    attr[0].config = PERF_COUNT_HW_CPU_CYCLES; /* generic PMU event*/
    attr[0].disabled = 0;
    fd[0] = perf_event_open(&attr[0], getpid(), -1, -1, 0);
    if (fd[0] < 0) {
        perror("Opening performance counter");
    }

    attr[1].type = PERF_TYPE_HARDWARE;
    attr[1].config = PERF_COUNT_HW_BRANCH_MISSES; /* generic PMU event*/
    attr[1].disabled = 0;
    fd[1] = perf_event_open(&attr[1], getpid(), -1, -1, 0);
    if (fd[1] < 0) {
        perror("Opening performance counter");
    }

    attr[2].type = PERF_TYPE_HARDWARE;
    attr[2].config = PERF_COUNT_HW_BRANCH_INSTRUCTIONS; /* generic PMU event*/
    attr[2].disabled = 0;
    fd[2] = perf_event_open(&attr[2], getpid(), -1, -1, 0);
    if (fd[2] < 0) {
        perror("Opening performance counter");
    }

    attr[3].type = PERF_TYPE_HARDWARE;
    attr[3].config = PERF_COUNT_HW_INSTRUCTIONS; /* generic PMU event*/
    attr[3].disabled = 0;
    fd[3] = perf_event_open(&attr[3], getpid(), -1, -1, 0);
    if (fd[3] < 0) {
        perror("Opening performance counter");
    }

    // Tell Linux to start counting events
    asm volatile("nop;");  // pseudo-barrier
    rc = read(fd[0], &val1[0], sizeof(val1[0]));
    assert(rc);
    rc = read(fd[1], &val1[1], sizeof(val1[1]));
    assert(rc);
    rc = read(fd[2], &val1[2], sizeof(val1[2]));
    assert(rc);
    rc = read(fd[3], &val1[3], sizeof(val1[3]));
    assert(rc);
    asm volatile("nop;");  // pseudo-barrier

    // The loop we are measuring
    for (i = 0; i < 100000; i++) {
        if (i % 3 == 0) {
            y += 1;
        }
    }

    // Read the counter
    asm volatile("nop;");  // pseudo-barrier
    rc = read(fd[0], &val2[0], sizeof(val2[0]));
    assert(rc);
    rc = read(fd[1], &val2[1], sizeof(val2[1]));
    assert(rc);
    rc = read(fd[2], &val2[2], sizeof(val2[2]));
    assert(rc);
    rc = read(fd[3], &val2[3], sizeof(val2[3]));
    assert(rc);
    assert(rc);
    asm volatile("nop;");  // pseudo-barrier

    // Close the counter
    close(fd[0]);
    close(fd[1]);
    close(fd[2]);
    close(fd[3]);

    printf("CPU Cycles:           %lu \n", val2[0] - val1[0]);
    printf("Instructions:         %lu \n", val2[3] - val1[3]);
    printf("IPC:                  %lf\n", ((double)val2[3] - val1[3]) / (val2[0] - val1[0]));
    printf("Branch misses:        %lu \n", val2[1] - val1[1]);
    printf("Branch instructions:  %lu \n", val2[2] - val1[2]);
    printf("Branch mispred. rate: %lf%%\n", 100.0 * ((double)val2[1] - val1[1]) / (val2[2] - val1[2]));
    printf("\n");

    return 0;
}