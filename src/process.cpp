
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
#include "proc_file.h"


bool is_process_runing(pid_t pid) {
    int rc = waitpid(pid, NULL, WNOHANG);
    if (rc == 0) {
        return true;
    } else if (rc > 0) {
        return false;
    } else {
        eprintf("pid not valid\n");
        return false;
    }
}



int run_cmd(char *exe_cmd, struct process_struct *process) {
    if (exe_cmd == NULL) {
        return -1;
    }
    if (pipe(process->pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Child process
        close(process->pipefd[0]);                // 关闭读端
        dup2(process->pipefd[1], STDOUT_FILENO);  // 标准输出重定向到写端
        dup2(process->pipefd[1], STDERR_FILENO);  // 可选：标准错误也重定向
        close(process->pipefd[1]);

        execvp(exe_cmd, &exe_cmd);  // 执行目标程序
        // execvp 返回说明出错
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        close(process->pipefd[1]);
        process->pid = pid;
        // 等待子进程进入可读 maps 状态
        if (wait_proc_ready(pid) == -1) {
            fprintf(stderr, "Process %d failed to exec within timeout.\n", pid);
            return -1;
        }
        return 0;
    }
}