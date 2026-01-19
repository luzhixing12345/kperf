#include "perf.h"
#include "log.h"
#include "utils.h"
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXN 512

int cpu_nums;
struct perf_sample_table *pst;

extern void int_exit(int _);
extern int need_kernel_callchain;

/* ================= perf helpers ================= */

static long perf_event_open(struct perf_event_attr *attr, pid_t _pid, int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, attr, _pid, cpu, group_fd, flags);
}

int init_perf_sample_table(struct perf_sample_table *table) {
    table->size = 0;
    table->capacity = SAMPLE_CAPACITY;
    table->samples = malloc(sizeof(struct perf_sample) * table->capacity);
    if (!table->samples) {
        perror("malloc");
        ERR("perf sample malloc failed");
        exit(1);
    }
    return 0;
}

void free_perf_sample_table(struct perf_sample_table *table) {
    for (int i = 0; i < table->size; i++) {
        free(table->samples[i].ips);
    }
    free(table->samples);
}

void add_perf_sample(struct perf_sample_table *st, uint32_t pid, uint32_t tid, uint64_t nr, uint64_t *ips) {
    if (unlikely(st->size == st->capacity)) {
        st->capacity *= SAMPLE_GROWTH_FACTOR;
        st->samples = realloc(st->samples, sizeof(struct perf_sample) * st->capacity);
        if (!st->samples) {
            perror("realloc");
            ERR("perf sample realloc failed");
            exit(1);
        }
        DEBUG("realloc perf sample table to %d", st->capacity);
    }
    struct perf_sample *ps = &st->samples[st->size];
    ps->pid = pid;
    ps->pid = pid;
    ps->tid = tid;
    ps->nr = nr;
    ps->ips = malloc(sizeof(uint64_t) * nr);
    memcpy(ps->ips, ips, sizeof(uint64_t) * nr);
    st->size++;
}

int process_event(struct perf_event_header *hdr, struct perf_sample_table *st) {
    /*
     * sample record format
     *
     * struct perf_event_header header;
     *                      // PERF_SAMPLE_TID
     * u32 pid;             // process id
     * u32 tid;             // thread id
     *                      // PERF_SAMPLE_CALLCHAIN
     * u64 nr;              // callchain depth
     * u64 ips[nr];         // callchain addrs
     */
    if (hdr->type == PERF_RECORD_SAMPLE) {
        uint8_t *p = (uint8_t *)hdr + sizeof(*hdr);
        uint32_t pid = *((uint32_t *)p);
        uint32_t tid = *((uint32_t *)(p + sizeof(uint32_t)));
        uint64_t nr = *((uint64_t *)(p + sizeof(uint32_t) * 2));
        uint64_t *ips = (uint64_t *)(p + sizeof(uint32_t) * 2 + sizeof(uint64_t));
        add_perf_sample(st, pid, tid, nr, ips);
        return hdr->size;
    }
    return 0;
}

/* ================= perf fd context ================= */

struct perf_fd_ctx {
    int fd;
    void *addr;
    uint64_t tail;
};

/* ================= sample thread ================= */

struct sample_thread_arg {
    int epfd;
};

void *sample_handler(void *arg) {
    struct sample_thread_arg *st = arg;
    int max_events = cpu_nums;
    struct epoll_event events[max_events];

    for (;;) {
        int n = epoll_wait(st->epfd, events, max_events, -1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            struct perf_fd_ctx *ctx = events[i].data.ptr;
            struct perf_event_mmap_page *mp;

            int event_size;

            mp = (struct perf_event_mmap_page *)ctx->addr;

            // stop kernel to add new data to ring buffer
            // ioctl(ctx->fd, PERF_EVENT_IOC_PAUSE_OUTPUT, 1);

            uint8_t *data = (uint8_t *)mp + mp->data_offset;
            uint64_t head = __atomic_load_n(&mp->data_head, __ATOMIC_ACQUIRE);
            uint64_t tail = ctx->tail;
            uint64_t size = mp->data_size;

            while (tail < head) {
                struct perf_event_header *hdr = (struct perf_event_header *)(data + (tail & (size - 1)));
                event_size = process_event(hdr, pst);

                if (event_size <= 0) {
                    /* resync */
                    tail = head;
                    break;
                }
                tail += event_size;
            }

            ctx->tail = tail;
            // ioctl(ctx->fd, PERF_EVENT_IOC_PAUSE_OUTPUT, 0);
        }
    }
    free(st);
    return NULL;
}

/* ================= main perf logic ================= */

int profile_process(int cgroup_fd, int sample_freq) {
    int i;
    int cpu_num;
    int epfd;
    long psize;
    pthread_t sample_thread;
    // struct sample_thread_arg st_arg;
    struct perf_fd_ctx *ctxs;
    int ctx_cnt = 0;

    /* -------- perf attr -------- */
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = PERF_TYPE_SOFTWARE;
    attr.size = sizeof(attr);
    attr.config = PERF_COUNT_SW_CPU_CLOCK;
    attr.sample_freq = sample_freq;
    attr.freq = 1;
    /* The ring buffer accumulates 16 samples before waking up epoll, reducing overhead. */
    attr.wakeup_events = 16;
    attr.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_CALLCHAIN;
    attr.sample_max_stack = 32;
    /*
     * sample record format
     *
     * pid, tid
     * nr
     * ip[0 ... nr-1]
     */

    if (!need_kernel_callchain)
        attr.exclude_callchain_kernel = 1;

    cpu_num = sysconf(_SC_NPROCESSORS_ONLN);
    ctxs = calloc(cpu_num, sizeof(*ctxs));
    psize = sysconf(_SC_PAGE_SIZE);
    DEBUG("cpu num: %d, page size: %ld\n", cpu_num, psize);

    /* -------- epoll -------- */
    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        perror("epoll_create1");
        return -1;
    }

    /* -------- attach perf events -------- */
    for (i = 0; i < cpu_num; i++) {
        int fd;
        void *addr;
        struct epoll_event ev;

        // create perf event for each cpu instead of use cpu = -1
        // to avoid multi-cpu write ring buffer
        fd = perf_event_open(&attr, cgroup_fd, i, -1, PERF_FLAG_FD_CLOEXEC | PERF_FLAG_PID_CGROUP);
        if (fd < 0) {
            WARNING("fail to open perf event on cpu %d\n", i);
            continue;
        }

        addr = mmap(NULL, (1 + MAXN) * psize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED) {
            perror("mmap");
            close(fd);
            continue;
        }

        ctxs[ctx_cnt].fd = fd;
        ctxs[ctx_cnt].addr = addr;
        ctxs[ctx_cnt].tail = 0;

        ev.events = EPOLLIN;
        ev.data.ptr = &ctxs[ctx_cnt];

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
            perror("epoll_ctl");
            munmap(addr, (1 + MAXN) * psize);
            close(fd);
            continue;
        }

        ctx_cnt++;
    }

    if (ctx_cnt == 0) {
        fprintf(stderr, "no perf events attached\n");
        return -1;
    }

    /* -------- start sample thread -------- */
    /* set global cpu count for sample thread max events */
    cpu_nums = ctx_cnt;

    struct sample_thread_arg *st_argp = malloc(sizeof(*st_argp));
    if (!st_argp) {
        perror("malloc");
        return -1;
    }
    st_argp->epfd = epfd;
    pthread_create(&sample_thread, NULL, sample_handler, st_argp);
    pthread_detach(sample_thread);

    return 0;
}
