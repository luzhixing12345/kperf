

#include <poll.h>
#include <limits.h>
#include <unistd.h>
#include <clib/clib.h>
#include <cstddef>
#include <cstdio>
#include <map>
#include <string>
#include <unordered_map>
#include "node.h"
#include "elf.h"
#include "process.h"
#include "perf.h"
#include "common.h"
#include "cgroup.h"
#include "symbol.h"

int check_kernel_callchain = 0;

std::unordered_map<int, STORE_T *> pid_symbols;
K_STORE_T *kernel_symbols = NULL;
STORE_T *user_symbols = NULL;

struct pollfd polls[MAXCPU];
// res for cleanup
static long long psize;
std::map<int, std::pair<void *, long long>> res;
std::unordered_map<unsigned long long, std::string> unknowns;
extern TNode *gnode;
int pid = 0;
int timeout = -1;
extern int kperf_cgroup_fd;
extern int use_cgroup;

int is_over = 0;
pthread_t display_data_thread;

char *get_language() {
    // check locale
    char *lang = getenv("LANG");
    if (!lang) {
        return lang;
    }
    return "en_US.UTF-8";
}

void int_exit(int _) {
    is_over = 1;
    pthread_join(display_data_thread, NULL);
    for (auto x : res) {
        auto y = x.second;
        void *addr = y.first;
        munmap(addr, (1 + MAXN) * psize);
        close(x.first);
    }
    res.clear();
    if (gnode != NULL) {
        FILE *fp = fopen("./report.html", "w");
        if (fp) {
            fprintf(fp, "<html lang=\"%s\">\n", get_language());
            fprintf(fp, "<head>\n");
            fprintf(fp, "<meta charset=\"UTF-8\">\n");
            fprintf(fp, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n");
            fprintf(fp, "<meta name=\"viewport\" content=\"width=device-width\">\n");
            fprintf(fp, "<title>profile report</title>\n");
            fprintf(fp, "<link rel=\"stylesheet\" href=\"report.css\">\n");
            fprintf(fp, "</head>\n");

            fprintf(fp, "<ul class=\"tree\">\n");
            gnode->printit(fp, 0);
            fprintf(fp, "</ul>\n");
            fprintf(fp, "<script src=\"report.js\"> </script>\n");
            fclose(fp);
            kprintf("collected %d samples, save report in report.html\n", gnode->c);
        }
        gnode = NULL;
    } else {
        kprintf("no report\n");
    }

    if (unknowns.size() > 0) {
        printf("---------------------unknowns-----------------\n");
        for (auto x = unknowns.begin(); x != unknowns.end(); x++) {
            printf("0x%llx  --?>  %s\n", (*x).first, (*x).second.c_str());
        }
    }
    printf("---------------------done---------------------\n");
    exit(0);
}
/*
perf call chain process
For now, if a address would not be located to some function, the address would
be skipped.
 */
int process_event(char *base, unsigned long long size, unsigned long long offset) {
    struct perf_event_header *p = NULL;
    offset %= size;
    // assuming the header would fit within size
    p = (struct perf_event_header *)(base + offset);
    offset += sizeof(*p);
    if (offset >= size)
        offset -= size;
    if (p->type != PERF_RECORD_SAMPLE)
        return p->size;
    // pid, tip;
    offset += 8;
    if (offset >= size)
        offset -= size;
    unsigned long long nr = *((unsigned long long *)(base + offset));
    offset += 8;
    if (offset >= size)
        offset -= size;
    if (nr > 128)
        return -1;
    unsigned long long addr, o, addr0;
    if (nr) {
        if (gnode == NULL)
            gnode = new TNode();
        TNode *r = gnode;
        addr0 = *((unsigned long long *)(base + offset));
        char user_mark = 0, start_mark = 0;
        for (int i = nr - 1; i >= 0; i--) {
            o = i * 8 + offset;
            if (o >= size)
                o -= size;
            addr = *((unsigned long long *)(base + o));
            if (addr == 0)
                continue;  // something wrong?
            if ((addr >> 56) == (addr0 >> 56) && (p->misc & PERF_RECORD_MISC_KERNEL)) {
                // skip the cross line command, no idear how to correctly resolve it
                // now.
                if (user_mark) {
                    user_mark = 0;
                    continue;
                }
                // check in kernel
                if (kernel_symbols && !kernel_symbols->empty()) {
                    auto x = kernel_symbols->upper_bound(addr);
                    if (x == kernel_symbols->begin()) {
                        // sprintf(bb, "0x%llx", addr); r = r->add(string(bb));
                        r = r->add(std::string("unknown"));
                    } else {
                        x--;
                        r = r->add((*x).second);
                    }
                } else {
                    // sprintf(bb, "0x%llx", addr); r = r->add(string(bb));
                    r = r->add(std::string("unknown"));
                }
            } else {
                if (check_kernel_callchain)
                    continue;
                if (user_symbols) {
                    auto x = user_symbols->upper_bound(addr);
                    if (x == user_symbols->begin()) {
                        // sprintf(bb, "0x%llx", addr); r = r->add(string(bb));
                        if (start_mark) {
                            auto y = (*x).second;
                            r = r->add(y.first + "?");
                        }
                    } else {
                        x--;
                        auto y = (*x).second;
                        if (y.second && addr > (*x).first + y.second) {
                            // r = r->add(y.first);
                            // sprintf(bb, "0x%llx", addr); r = r->add(string(bb));
                            if (start_mark) {
                                x++;
                                if (x == user_symbols->end())
                                    r = r->add(y.first + "??");
                                else {
                                    auto z = (*x).second;
                                    r = r->add(y.first + "?" + z.first);
                                }
                            }
                        } else {
                            start_mark = 1;
                            r = r->add(y.first);
                        }
                    }
                } else {
                    // sprintf(bb, "0x%llx", addr); r = r->add(string(bb));
                    // r = r->add(string("unknown"));
                }
                user_mark = 1;
            }
        }
    }
    return p->size;
}

void handle_alarm(int sig) {
    kprintf("timeout\n");
    if (is_process_runing(pid)) {
        kill(pid, SIGKILL);
        kprintf("killed pid %d\n", pid);
    }
    int_exit(0);
}

void *show_collected_data(void *arg) {
    while (!is_over) {
        kprintf("collecting samples %d\n", gnode ? gnode->c : 0);
        sleep(1);
        if (is_over) {
            return NULL;
        }
        // clear the line
        printf("\033[F\033[K");
    }

    return NULL;
}

int main(int argc, const char *argv[]) {
    int *cgroup_pids = NULL;
    char *exe_cmd = NULL;
    int enable_debug = 0;
    int rc = 0;
    // log_init("/tmp/kperf.log");
    log_set_level(LOG_INFO);
    argparse_option options[] = {
        ARG_INT(&pid,
                "-p",
                "--pid",
                "pid > 0 collects program and kernel function callchain       pid < 0 "
                "collects kernel function callchain only",
                " <pid>",
                "pid"),
        ARG_INTS_GROUP(&cgroup_pids, NULL, NULL, "multiple pids", " <pid> ...", "cgroup_pids"),
        ARG_BOOLEAN(&use_cgroup, NULL, "--cgroup", "collect process in cgroup", NULL, "cgroup"),
        ARG_BOOLEAN(&check_kernel_callchain, "-k", "--kernel", "kernel callchain only", NULL, "kernel"),
        ARG_STR(&exe_cmd, NULL, "--", "command to run", NULL, "command"),
        ARG_INT(&timeout, "-t", "--timeout", "maximum monitor time in seconds", " <s>", "timeout"),
        ARG_BOOLEAN(&enable_debug, "-d", "--debug", "enable debug", NULL, "debug"),
        ARG_BOOLEAN(NULL, "-h", "--help", "show help information", NULL, "help"),
        ARG_BOOLEAN(NULL, "-v", "--version", "show version", NULL, "version"),
        ARG_END()};

    argparse parser;
    argparse_init(&parser, options, ARGPARSE_ENABLE_CMD);
    argparse_describe(
        &parser, "kperf", "kernel callchain profiler", "Full documentation: https://github.com/luzhixing12345/kperf");
    argparse_parse(&parser, argc, argv);

    if (arg_ismatch(&parser, "help")) {
        argparse_info(&parser);
        free_argparse(&parser);
        return 0;
    }

    if (arg_ismatch(&parser, "version")) {
        printf("kperf version %s\n", CONFIG_VERSION);
        free_argparse(&parser);
        return 0;
    }

    if (check_kernel_callchain) {
        rc = load_kernel_symbol();
        if (rc < 0) {
            eprintf("load kernel symbol failed\n");
            free_argparse(&parser);
            return 1;
        }
    }

    if (exe_cmd && pid) {
        eprintf("You can't specify both pid and command.\n");
        free_argparse(&parser);
        return 1;
    }

    int cgroup_pid_num = arg_ismatch(&parser, "cgroup_pids");
    if (use_cgroup) {
        if (!cgroup_pid_num) {
            eprintf("You must specify cgroup pids.\n");
            free_argparse(&parser);
            return 1;
        }
        create_cgroup(cgroup_pids, cgroup_pid_num);
        pid = cgroup_pids[0];
    } else if (pid) {
        if (!is_process_runing(pid)) {
            eprintf("pid %d is not running\n", pid);
            free_argparse(&parser);
            return 1;
        }
    } else if (exe_cmd) {;
        struct cmd_arg args;
        parseline(exe_cmd, &args);
        rc = load_user_symbol(args.argv[0]);
        if (rc < 0) {
            eprintf("load user symbol failed\n");
            free_argparse(&parser);
            return 1;
        }
        pid = run_cmd(&args);
        free_cmd_arg(&args);
    }

    user_symbols = load_symbol_pid(pid);
    kprintf("loaded user symbols [%lu]\n", user_symbols->size());
    kernel_symbols = load_kernel();
    kprintf("loaded kernel symbols [%lu]\n", kernel_symbols->size());

    signal(SIGINT, int_exit);
    signal(SIGTERM, int_exit);
    signal(SIGALRM, handle_alarm);

    if (timeout > 0) {
        kprintf("setup timer for %d seconds\n", timeout);
        alarm(timeout);
    }

    int poll_num = 0;
    void *addr;
    psize = sysconf(_SC_PAGE_SIZE);  // getpagesize();
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

    int perf_flags = PERF_FLAG_FD_CLOEXEC;
    if (use_cgroup) {
        perf_flags |= PERF_FLAG_PID_CGROUP;
    }
    if (check_kernel_callchain) {
        attr.exclude_callchain_user = 1;
    }
    for (int i = 0; i < cpu_num && i < MAXCPU; i++) {
        // printf("attaching cpu %d\n", i);
        int fd = perf_event_open(&attr, use_cgroup ? kperf_cgroup_fd : pid, i, -1, perf_flags);
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
        res[fd] = std::make_pair(addr, 0);
        polls[poll_num].fd = fd;
        polls[poll_num].events = POLLIN;
        polls[poll_num].revents = 0;
        poll_num++;
    }
    if (poll_num == 0) {
        printf("no cpu event attached at all\n");
        return 1;
    }

    // 创建一个线程
    if (pthread_create(&display_data_thread, NULL, show_collected_data, NULL) != 0) {
        perror("pthread_create failed");
        return 1;
    }

    unsigned long long head;
    int event_size;
    struct perf_event_mmap_page *mp;
    while (1) {
        if (poll(polls, poll_num, 0) > 0) {
            for (int i = 0; i < poll_num; i++) {
                if ((polls[i].revents & POLLIN) == 0)
                    continue;
                int fd = polls[i].fd;
                addr = res[fd].first;
                mp = (struct perf_event_mmap_page *)addr;
                head = res[fd].second;
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
                res[fd].second = mp->data_head;
                ioctl(fd, PERF_EVENT_IOC_PAUSE_OUTPUT, 0);
            }
        }

        if (is_process_runing(pid) <= 0) {
            kprintf("process finished\n");
            int_exit(0);
        }
        // printf("samples: %d\n", gnode->c);
    }

    if (use_cgroup) {
        close(kperf_cgroup_fd);
    }
    free_argparse(&parser);
    int_exit(0);
    return 0;
}
