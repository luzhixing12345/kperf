
#pragma once

#include <sys/types.h>


struct process_struct {
    int pid;
    int stdout_fd;

    // private data
    int pipefd[2];
};

int run_cmd(struct cmd_arg *args);

bool is_process_runing(pid_t pid);