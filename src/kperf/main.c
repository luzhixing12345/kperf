
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "argparse.h"
#include "log.h"
#include "symbol.h"
#include "utils.h"

#define MAXN   512
#define MAXCPU 128

int timeout = -1;
int check_kernel_callchain = 0;
struct symbol_table *kst = NULL;
struct symbol_table *ust = NULL;
pid_t pid;

struct pollfd polls[MAXCPU];

void int_exit(int _) {
}

static long perf_event_open(struct perf_event_attr *perf_event, pid_t _pid, int cpu, int group_fd,
                            unsigned long flags) {
    return syscall(__NR_perf_event_open, perf_event, _pid, cpu, group_fd, flags);
}

int process_event(char *base, unsigned long long size, unsigned long long offset) {
    return 0;
}

int main(int argc, char *argv[]) {
    char *exe_cmd = NULL;
    int enable_debug = 0;
    int rc = 0;
    argparse parser;
    argparse_option options[] = {
        ARG_INT(&pid, "-p", "--pid", "process id to monitor", " <pid>", "pid"),
        ARG_BOOLEAN(&check_kernel_callchain, "-k", "--kernel", "kernel callchain only", NULL, "kernel"),
        ARG_STR(&exe_cmd, NULL, "--", "command to run", NULL, "command"),
        ARG_INT(&timeout, "-t", "--timeout", "maximum monitor time in seconds", " <s>", "timeout"),
        ARG_BOOLEAN(&enable_debug, "-d", "--debug", "enable debug", NULL, "debug"),
        ARG_BOOLEAN(NULL, "-h", "--help", "show help information", NULL, "help"),
        ARG_BOOLEAN(NULL, "-v", "--version", "show version", NULL, "version"),
        ARG_END()};

    if (!is_root()) {
        printf("kperf must be run as root\n");
        return 1;
    }

    argparse_init(&parser, options, ARGPARSE_ENABLE_CMD);
    argparse_describe(&parser,
                      "kperf",
                      "linux kernel and user space program profiler",
                      "Full documentation: https://github.com/luzhixing12345/kperf");
    argparse_parse(&parser, argc, argv);

    if (arg_ismatch(&parser, "help")) {
        argparse_info(&parser);
        free_argparse(&parser);
        return 0;
    }

    if (arg_ismatch(&parser, "version")) {
        printf("kperf version %s\n", VERSION);
        free_argparse(&parser);
        return 0;
    }

    if (enable_debug) {
        log_set_level(LOG_DEBUG);
    }

    kst = malloc(sizeof(struct symbol_table));
    ust = malloc(sizeof(struct symbol_table));
    init_symbol_table(kst);
    init_symbol_table(ust);

    if (check_kernel_callchain) {
        rc = load_kernel_symbols(kst);
        if (rc < 0) {
            goto cleanup;
        }
    }

    // run a program: ./kperf -- <command>
    int status;
    if (exe_cmd) {
        pid = fork();
        if (pid == 0) {
            /* child */
            ptrace(PTRACE_TRACEME, 0, NULL, NULL);
            raise(SIGSTOP);

            char **child_argv = parse_cmdline(exe_cmd);
            execvp(child_argv[0], child_argv);
            perror("execvp");
            exit(1);
        }
        
        waitpid(pid, &status, 0);
        printf("[parent] child stopped (pre-exec)\n");
        ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACEEXEC);
        ptrace(PTRACE_CONT, pid, 0, 0);

        // wait for exec
        waitpid(pid, &status, 0);
        if (WIFSTOPPED(status)) {
            printf("[parent] child stopped after exec\n");
        }
        load_user_symbols(ust, pid);
        ptrace(PTRACE_CONT, pid, 0, 0);
        waitpid(pid, &status, 0);
    } else {
        load_user_symbols(ust, pid);
    }

    goto cleanup;

    return 0;

    pid = atoi(argv[1]);
    // if (pid<0) { gflag_kernel_only = 1; pid=-pid; }
    if (pid == 0) {
        printf("invalid pid %s\n", argv[1]);
        return 1;
    }
    // find cgroup
    char xb[256], xb2[256];
    int i, j, k, fd;
    void *addr;
    sprintf(xb, "/proc/%d/cgroup", pid);
    FILE *fp = fopen(xb, "r");
    // if (fp==NULL) error("fail to open cgroup file");
    char *p;
    xb2[0] = 0;
    int cgroup_name_len = 0;
    while (1) {
        p = fgets(xb, sizeof(xb), fp);
        if (p == NULL)
            break;
        i = 0;
        while (p[i] && p[i] != ':') i++;
        if (p[i] == 0)
            continue;
        if (strstr(p, "perf_event")) {
            i++;
            while (p[i] != ':' && p[i]) i++;
            if (p[i] != ':')
                continue;
            i++;
            j = i;
            while (p[j] != '\r' && p[j] != '\n' && p[j] != 0) j++;
            p[j] = 0;
            sprintf(xb2, "/sys/fs/cgroup/perf_event%s", p + i);
            cgroup_name_len = j - i;
            break;
        } else if (p[i + 1] == ':') {
            i += 2;
            j = i;
            while (p[j] != '\r' && p[j] != '\n' && p[j] != 0) j++;
            p[j] = 0;
            sprintf(xb2, "/sys/fs/cgroup/%s", p + i);
            cgroup_name_len = j - i;
        }
    }
    fclose(fp);
    // if (xb2[0]==0) error("no proper cgroup found\n");
    if (cgroup_name_len < 2) {
        printf("cgroup %s seems to be root, not allowed\n", xb2);
        return -1;
    }
    printf("try to use cgroup %s\n", xb2);
    int cgroup_id = open(xb2, O_CLOEXEC);
    if (cgroup_id <= 0) {
        perror("error open cgroup dir");
        return 1;
    }
    // start perf event
    long psize = sysconf(_SC_PAGE_SIZE);  // getpagesize();
    int cpu_num = sysconf(_SC_NPROCESSORS_ONLN);
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = PERF_TYPE_SOFTWARE;
    attr.size = sizeof(attr);
    attr.config = PERF_COUNT_SW_CPU_CLOCK;
    attr.sample_freq = 777;  // adjust it
    attr.freq = 1;
    attr.wakeup_events = 16;
    attr.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_CALLCHAIN;
    attr.sample_max_stack = 32;
    // if (gflag_kernel_only) attr.exclude_callchain_user = 1;
    for (i = 0, k = 0; i < cpu_num && i < MAXCPU; i++) {
        printf("attaching cpu %d\n", i);
        fd = perf_event_open(&attr, cgroup_id, i, -1, PERF_FLAG_FD_CLOEXEC | PERF_FLAG_PID_CGROUP);
        if (fd < 0) {
            perror("fail to open perf event");
            continue;
        }
        addr = mmap(NULL, (1 + MAXN) * psize, PROT_READ, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED) {
            perror("mmap failed");
            close(fd);
            continue;
        }
        // res[fd] = make_pair(addr, 0);
        polls[k].fd = fd;
        polls[k].events = POLLIN;
        polls[k].revents = 0;
        k++;
    }
    if (k == 0) {
        printf("no cpu event attached at all\n");
        return 1;
    }

    signal(SIGINT, int_exit);
    signal(SIGTERM, int_exit);

    unsigned long long head;
    int event_size;
    struct perf_event_mmap_page *mp;
    while (poll(polls, k, -1) > 0) {
        // printf("wake\n");
        for (i = 0; i < k; i++) {
            if ((polls[i].revents & POLLIN) == 0)
                continue;
            fd = polls[i].fd;
            // addr = res[fd].first;
            mp = (struct perf_event_mmap_page *)addr;
            // head = res[fd].second;
            ioctl(fd, PERF_EVENT_IOC_PAUSE_OUTPUT, 1);
            if (head > mp->data_head)
                head = mp->data_head;
            head = mp->data_head - ((mp->data_head - head) % mp->data_size);
            while (head < mp->data_head) {
                event_size = process_event((char *)addr + mp->data_offset, mp->data_size, head);
                if (event_size < 0) {
                    // resync
                    head = mp->data_head;
                    break;
                }
                head += event_size;
            }
            // res[fd].second = mp->data_head;
            ioctl(fd, PERF_EVENT_IOC_PAUSE_OUTPUT, 0);
        }
    }

    int_exit(0);

cleanup:
    free_argparse(&parser);
    free_symbol_table(kst);
    free_symbol_table(ust);

    return 0;
}