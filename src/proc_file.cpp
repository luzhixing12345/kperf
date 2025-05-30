
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
#include "proc_file.h"

#define MAX_WAIT_MS 5000        // 最多等待 5000ms 子进程进入 exec
#define WAIT_INTERVAL_US 10000  // 每次等待 10ms

int wait_proc_ready(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    int retries = MAX_WAIT_MS * 1000 / WAIT_INTERVAL_US;

    while (retries--) {
        int fd = open(path, O_RDONLY);
        if (fd != -1) {
            close(fd);
            return 0;  // 成功打开 maps，说明子进程 exec 完成
        }
        if (errno == ENOENT) {
            // 进程可能还没启动或已经异常退出
            return -1;
        }
        usleep(WAIT_INTERVAL_US);
    }
    return -1;  // 等待超时
}