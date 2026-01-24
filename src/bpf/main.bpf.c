#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

const pid_t pid_filter = 0;

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("tp/syscalls/sys_enter_write")
int handle_tp(void* ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    pid_t pid = pid_tgid >> 32;

    if (pid_filter && pid != pid_filter)
        return 0;

    bpf_printk("BPF triggered sys_enter_write from PID %d.\n", pid);
    return 0;
}
