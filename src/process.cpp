
#include "process.h"
#include <asm/unistd.h>
#include <elf.h>
#include <fcntl.h>
#include <sys/types.h>
#include <cerrno>
#include "common.h"
#include <stdio.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <clib/clib.h>

#define MAX_WAIT_MS 5000        // 最多等待 5000ms 子进程进入 exec
#define WAIT_INTERVAL_US 10000  // 每次等待 10ms

bool is_process_runing(pid_t pid) {
    if (kill(pid, 0) == 0) {
        return true;
    } else {
        return false;
    }
}

int wait_proc_ready(pid_t pid) {
    int retries = MAX_WAIT_MS * 1000 / WAIT_INTERVAL_US;

    while (retries--) {
        if (is_process_runing(pid)) {
            return 0;  // 子进程正在运行
        }
        usleep(WAIT_INTERVAL_US);
    }
    return -1;  // 等待超时
}

int run_cmd(struct cmd_arg *args) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Child process
        execvp(args->argv[0], args->argv);  // 执行目标程序
        // execvp 返回说明出错
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // 等待子进程进入可读 maps 状态
        if (wait_proc_ready(pid) == -1) {
            fprintf(stderr, "Process %d failed to exec within timeout.\n", pid);
            return -1;
        }
        kprintf("Process %d is ready.\n", pid);
        return pid;
    }
}