
#pragma once

#include <sys/types.h>


struct process_struct {
    int pid;
    int stdout_fd;

    // private data
    int pipefd[2];
};

int run_cmd(char *exe_cmd, struct process_struct *process);

bool is_process_runing(pid_t pid);