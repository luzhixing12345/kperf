#include <stdio.h>
#include <bpf/libbpf.h>
#include <unistd.h>

int load_bpf_program(char *file_path) {
    struct bpf_object *obj;
    int err;

    obj = bpf_object__open_file(file_path, NULL);
    if (!obj) {
        fprintf(stderr, "Failed to open BPF object\n");
        return 1;
    }

    err = bpf_object__load(obj);
    if (err) {
        fprintf(stderr, "Failed to load BPF object: %d\n", err);
        return 1;
    }

    struct bpf_program *prog = bpf_object__find_program_by_name(obj, "handle_tp");
    if (!prog) {
        fprintf(stderr, "Cannot find BPF program\n");
        return 1;
    }

    printf("prog fd = %d\n", bpf_program__fd(prog));
    printf("prog type = %d\n", bpf_program__get_type(prog));

    // attach tracepoint
    struct bpf_link *link;
    link = bpf_program__attach_tracepoint(prog, "syscalls", "sys_enter_write");
    if (!link) {
        fprintf(stderr, "Failed to attach tracepoint\n");
        return 1;
    }

    printf("BPF program loaded. Check trace_pipe for events.\n");
    printf("Press Ctrl-C to exit.\n");

    while (1) sleep(1);

    bpf_link__destroy(link);
    bpf_object__close(obj);

    return 0;
}
