
#include "cgroup.h"
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common.h"

int kperf_cgroup_fd;
int use_cgroup = 0;

static int remove_exist_cgroup_pids(int fd) {
    // Read current cgroup pids into buffer
    char buf[1024];
    ssize_t len = read(fd, buf, sizeof(buf) - 1);
    if (len == -1) {
        perror("read");
        close(fd);
        return -1;
    } else if (len == 0) {
        // cgroup.procs is empty
        return 0;
    }
    buf[len] = '\0';  // Null-terminate the string

    // Parse the current pids and move them to the root cgroup
    char cgroup_path[PATH_MAX];
    snprintf(cgroup_path, sizeof(cgroup_path), "/sys/fs/cgroup/cgroup.procs");
    int root_fd = open(cgroup_path, O_WRONLY);
    if (root_fd == -1) {
        perror("open root cgroup.procs");
        close(fd);
        return -1;
    }

    // $ sudo cat /sys/fs/cgroup/kperf/cgroup.procs
    // 1
    // 3

    char *pid_str = strtok(buf, "\n");
    while (pid_str != NULL) {
        pid_t pid = atoi(pid_str);
        if (pid > 0) {
            // Move the current PID to the root cgroup
            char pid_move[32];
            snprintf(pid_move, sizeof(pid_move), "%d", pid);
            if (write(root_fd, pid_move, strlen(pid_move)) == -1) {
                perror("write root cgroup.procs");
                close(root_fd);
                close(fd);
                return -1;
            }
        }
        pid_str = strtok(NULL, "\n");
        kprintf("move pid %d to root cgroup\n", pid);
    }
    close(root_fd);
    return 0;
}

int create_cgroup(int *cgroup_pids, int n) {
    // check if all pids are valid
    for (int i = 0; i < n; i++) {
        pid_t pid = cgroup_pids[i];
        if (pid < 0) {
            eprintf("invalid cgroup pid %d\n", pid);
            return -1;
        }
    }

    // cgroup v1 or v2
    const char *base_cgroup_path;
    if (access("/sys/fs/cgroup/cgroup.controllers", F_OK) == -1) {
        // v1
        base_cgroup_path = "/sys/fs/cgroup/cpu";
    } else {
        // v2
        base_cgroup_path = "/sys/fs/cgroup";
    }

    char cgroup_path[PATH_MAX];
    snprintf(cgroup_path, sizeof(cgroup_path), "%s/%s", base_cgroup_path, KPERF_CGROUP_NAME);

    struct stat st;
    if (stat(cgroup_path, &st) == -1) {
        if (mkdir(cgroup_path, 0777) == -1) {
            perror("mkdir");
            return -1;
        }
        kprintf("init kperf cgroup\n");
    }

    kperf_cgroup_fd = open(cgroup_path, O_CLOEXEC);
    if (kperf_cgroup_fd == -1) {
        perror("open");
        return -1;
    }

    snprintf(cgroup_path, sizeof(cgroup_path), "%s/%s/cgroup.procs", base_cgroup_path, KPERF_CGROUP_NAME);

    int fd = open(cgroup_path, O_RDWR);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    remove_exist_cgroup_pids(fd);

    char pid_str[32];
    for (int i = 0; i < n; i++) {
        snprintf(pid_str, sizeof(pid_str), "%d", cgroup_pids[i]);
        if (write(fd, pid_str, strlen(pid_str)) == -1) {
            kprintf("pid %d add to cgroup failed, check if it's a valid pid\n", cgroup_pids[i]);
        } else {
            kprintf("add %s to kperf cgroup\n", pid_str);
        }
    }
    close(fd);

    return 0;
}
