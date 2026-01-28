
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
#include <libgen.h>
#include <errno.h>
#include "argparse.h"
#include "build_html.h"
#include "http_server.h"
#include "log.h"
#include "config.h"
#include "symbol.h"
#include "utils.h"
#include "cgroup.h"
#include "perf.h"
#include "version.h"
#include "tui.h"

#define MAX_PIDS 1024

/* -------- global vars -------- */
int timeout = -1;
int want_exit = 0;
int enable_debug = 0;
struct symbol_table *kst = NULL;
struct symbol_table *ust = NULL;
extern struct perf_sample_table *pst;
extern int is_server_running;
int g_pid;
int pids[MAX_PIDS];
int pid_num = 0;
int enable_tui = 0;
int need_kernel_callchain = 0;

void int_exit(int signo) {
    DEBUG("kperf received signal %d, exit now\n", signo);
    want_exit = 1;
    if (is_server_running) {
        exit(0);
    } else {
        kill(g_pid, SIGTERM);
    }
}

int main(int argc, char *argv[]) {
    char *exe_cmd = NULL;
    int rc = 0;
    int sample_freq = 777;
    int http_port = 0;
    int only_launch_http_server = 0;
    argparse parser;
    argparse_option options[] = {
        ARG_INT(&g_pid, "-p", "--pid", "process id to monitor", " <pid>", "pid"),
        ARG_BOOLEAN(&need_kernel_callchain, "-k", "--kernel", "also check kernel callchain", NULL, "kernel"),
        ARG_STR(&exe_cmd, NULL, "--", "command to run", " <cmd>", "command"),
        ARG_INT(&sample_freq, "-F", "--freq", "sampling frequency [default 777 Hz]", " <Hz>", "freq"),
        ARG_INT(&timeout, "-t", "--timeout", "maximum monitor time in seconds", " <s>", "timeout"),
        ARG_BOOLEAN(&only_launch_http_server,
                    "-s",
                    "--server",
                    "only launch http server without doing anything else",
                    NULL,
                    "launch"),
        ARG_INT(&http_port, NULL, "--port", "http server port", " <port>", "port"),
        ARG_BOOLEAN(&enable_tui, NULL, "--tui", "use tui instead of html", NULL, "tui"),
        ARG_BOOLEAN(NULL, "-h", "--help", "show help information", NULL, "help"),
        ARG_BOOLEAN(NULL, "-v", "--version", "show version", NULL, "version"),
        ARG_BOOLEAN(&enable_debug, "-d", "--debug", "enable debug", NULL, "debug"),
        ARG_END()};

    argparse_init(&parser, options, ARGPARSE_ENABLE_CMD);
    argparse_describe(&parser,
                      "kperf",
                      "linux kernel and user space program profiler",
                      "Examples:\n"
                      "  sudo kperf -- ./a\t\t    run program and analysis\n"
                      "  sudo kperf -p `pidof a`\t    analysis running process\n\n"
                      "Full documentation: https://github.com/luzhixing12345/kperf");
    argparse_parse(&parser, argc, argv);

    if (arg_ismatch(&parser, "help") || (!exe_cmd && !g_pid && !only_launch_http_server)) {
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
        // enable_tui = 1;
    }

    if (only_launch_http_server) {
        char *assets[] = {
            KPERF_ETC_TPL_PATH "/index.js", KPERF_ETC_TPL_PATH "/index.css", KPERF_ETC_TPL_PATH "/favicon.svg"};
        for (int i = 0; i < sizeof(assets) / sizeof(*assets); i++) {
            char *src = assets[i];
            char dst[PATH_MAX];
            snprintf(dst, sizeof(dst), "%s/%s", KPERF_RESULTS_PATH, basename(src));
            if (copy_file(src, dst) < 0) {
                WARNING("failed to copy %s to %s: %s\n", src, dst, strerror(errno));
            } else {
                chmod(dst, 0666);
            }
        }
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
    signal(SIGINT, int_exit);
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
        g_pid = fork();
        if (g_pid == 0) {
            /* child */
            ptrace(PTRACE_TRACEME, 0, NULL, NULL);
            raise(SIGSTOP);

            char **child_argv = parse_cmdline(exe_cmd);
            execvp(child_argv[0], child_argv);
            perror("execvp");
            exit(1);
        }

        /* SIGSTOP */
        DEBUG("kperf pid = %d, child pid = %d\n", getpid(), g_pid);
        waitpid(g_pid, &status, 0);
        ptrace(PTRACE_SETOPTIONS, g_pid, 0, PTRACE_O_TRACEEXEC | PTRACE_O_TRACEEXIT | PTRACE_O_EXITKILL);
        ptrace(PTRACE_CONT, g_pid, 0, 0);

        /* wait for exec */
        waitpid(g_pid, &status, 0);

        pids[0] = g_pid;
        pid_num = 1;
        int cgroup_fd = setup_cgroup(pids, 1);
        if (cgroup_fd < 0) {
            goto cleanup;
        }
        profile_process(cgroup_fd, sample_freq);
        // start_http_server(http_port);

        ptrace(PTRACE_CONT, g_pid, 0, 0);
        while (1) {
            waitpid(g_pid, &status, 0);
            /* 1. tracee 正常退出 */
            if (WIFEXITED(status)) {
                DEBUG("tracee exited, status=%d\n", WEXITSTATUS(status));
                break;
            }

            /* 2. tracee 被信号杀死 */
            if (WIFSIGNALED(status)) {
                DEBUG("tracee killed by signal %d (%s)\n", WTERMSIG(status), strsignal(WTERMSIG(status)));
                break;
            }
            /* 3. tracee 停止（ptrace 的核心路径） */
            if (WIFSTOPPED(status)) {
                int sig = WSTOPSIG(status);

                if (sig == SIGTRAP && ((status >> 16) & 0xffff)) {
                    int event = (status >> 16) & 0xffff;

                    if (event == PTRACE_EVENT_EXIT) {
                        unsigned long exit_code;
                        ptrace(PTRACE_GETEVENTMSG, g_pid, 0, &exit_code);
                        DEBUG("tracee about to exit, code=%lu\n", exit_code);
                        load_user_symbols(ust, g_pid);

                        ptrace(PTRACE_CONT, g_pid, 0, 0);
                        break;
                    } else {
                        ERR("unknown event %d\n", event);
                    }
                }

                DEBUG("tracee stopped by signal %d (%s)\n", sig, strsignal(sig));

                ptrace(PTRACE_CONT, g_pid, 0, sig);
                continue;
            }
            ptrace(PTRACE_CONT, g_pid, 0, 0);
        }
    } else {
        load_user_symbols(ust, g_pid);
    }

    INFO("perf sample size = %d\n", pst->size);
    if (pst->size == 0) {
        goto cleanup;
    }
    if (!enable_tui) {
        rc = build_html(pst, ust, kst);
        if (rc < 0) {
            goto cleanup;
        }
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