
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "argparse.h"
#include "build_html.h"
#include "http_server.h"
#include "log.h"
#include "symbol.h"
#include "utils.h"
#include "cgroup.h"
#include "perf.h"
#include "version.h"
#include "tui.h"

#define MAX_PIDS 1024

/* -------- global vars -------- */
int timeout = -1;
struct symbol_table *kst = NULL;
struct symbol_table *ust = NULL;
extern struct perf_sample_table *pst;
pid_t pid;
int pids[MAX_PIDS];
int pid_num = 0;
int use_tui = 0;

void int_exit(int _) {
}

int main(int argc, char *argv[]) {
    char *exe_cmd = NULL;
    int enable_debug = 0;
    int rc = 0;
    int sample_freq = 777;
    int need_kernel_callchain = 0;
    int http_port = 0;
    int only_launch_http_server = 0;
    argparse parser;
    argparse_option options[] = {
        ARG_INT(&pid, "-p", "--pid", "process id to monitor", " <pid>", "pid"),
        ARG_BOOLEAN(&need_kernel_callchain, "-k", "--kernel", "kernel callchain only", NULL, "kernel"),
        ARG_STR(&exe_cmd, NULL, "--", "command to run", " <cmd>", "command"),
        ARG_INT(&sample_freq, "-F", "--freq", "sampling frequency [default 777 Hz]", " <Hz>", "freq"),
        ARG_INT(&timeout, "-t", "--timeout", "maximum monitor time in seconds", " <s>", "timeout"),
        ARG_BOOLEAN(&enable_debug, "-d", "--debug", "enable debug", NULL, "debug"),
        ARG_BOOLEAN(&only_launch_http_server,
                    "-s",
                    "--server",
                    "only launch http server without doing anything else",
                    NULL,
                    "launch"),
        ARG_INT(&http_port, NULL, "--port", "http server port", " <port>", "port"),
        ARG_BOOLEAN(&use_tui, "-T", "--tui", "use tui instead of html", NULL, "tui"),
        ARG_BOOLEAN(NULL, "-h", "--help", "show help information", NULL, "help"),
        ARG_BOOLEAN(NULL, "-v", "--version", "show version", NULL, "version"),
        ARG_END()};

    argparse_init(&parser, options, ARGPARSE_ENABLE_CMD);
    argparse_describe(&parser,
                      "kperf",
                      "linux kernel and user space program profiler",
                      "Examples:\n"
                      "  sudo kperf -- ./a\n"
                      "  sudo kperf -p `pidof a`\n\n"
                      "Full documentation: https://github.com/luzhixing12345/kperf");
    argparse_parse(&parser, argc, argv);

    if (arg_ismatch(&parser, "help") || (!exe_cmd && !pid && !only_launch_http_server)) {
        argparse_info(&parser);
        free_argparse(&parser);
        return 0;
    }

    if (arg_ismatch(&parser, "version")) {
        printf("kperf version v%s\n", get_version_str());
        free_argparse(&parser);
        return 0;
    }

    if (enable_debug) {
        log_set_level(LOG_DEBUG);
    }

    if (only_launch_http_server) {
        start_http_server(http_port);
        free_argparse(&parser);
        return 0;
    }

    kst = malloc(sizeof(struct symbol_table));
    ust = malloc(sizeof(struct symbol_table));
    pst = malloc(sizeof(struct perf_sample_table));
    init_symbol_table(kst);
    init_symbol_table(ust);
    init_perf_sample_table(pst);
    // signal(SIGINT, int_exit);
    // signal(SIGTERM, int_exit);

    if (!is_root()) {
        printf("kperf must be run as root\n");
        goto cleanup;
    }

    if (need_kernel_callchain) {
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

        /* SIGSTOP */
        waitpid(pid, &status, 0);
        ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACEEXEC | PTRACE_O_TRACEEXIT | PTRACE_O_EXITKILL);
        ptrace(PTRACE_CONT, pid, 0, 0);

        /* wait for exec */
        waitpid(pid, &status, 0);

        pids[0] = pid;
        pid_num = 1;
        int cgroup_fd = setup_cgroup(pids, 1);
        if (cgroup_fd < 0) {
            goto cleanup;
        }
        profile_process(cgroup_fd, sample_freq, need_kernel_callchain);
        // start_http_server(http_port);

        ptrace(PTRACE_CONT, pid, 0, 0);
        while (1) {
            waitpid(pid, &status, 0);
            if (WIFSTOPPED(status)) {
                int sig = WSTOPSIG(status);

                if (sig == SIGTRAP && ((status >> 16) & 0xffff) == PTRACE_EVENT_EXIT) {
                    unsigned long exit_code;
                    ptrace(PTRACE_GETEVENTMSG, pid, 0, &exit_code);
                    load_user_symbols(ust, pid);
                    DEBUG("tracee about to exit, code=%lu\n", exit_code);
                    break;
                } else {
                    DEBUG("tracee stopped, sig=%d\n", sig);
                    /* pass signal to child process */
                }
            }
            ptrace(PTRACE_CONT, pid, 0, 0);
        }
    } else {
        load_user_symbols(ust, pid);
    }

    INFO("perf sample size = %d\n", pst->size);
    if (!use_tui) {
        build_html(pst, ust, kst);
        start_http_server(http_port);
    } else {
        build_tui(pst, ust, kst);
    }
    INFO("done\n");

    // int_exit(0);

cleanup:
    free_argparse(&parser);
    free_symbol_table(kst);
    free_symbol_table(ust);
    free_perf_sample_table(pst);

    return 0;
}