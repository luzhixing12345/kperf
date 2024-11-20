
#include "process.h"
#include <asm/unistd.h>
#include <elf.h>
#include <fcntl.h>
#include <sys/types.h>
#include <cerrno>
#include "cgroup.h"

int is_process_runing(pid_t pid) {
    // Send signal 0 to check if the process is running
    if (kill(pid, 0) == 0) {
        return 1;  // Process is running
    } else if (errno == ESRCH) {
        return 0;  // Process does not exist
    } else {
        return -1;  // An error occurred (e.g., permission denied)
    }
}
