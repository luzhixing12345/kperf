import ctypes
import sys
import time
from pathlib import Path

# libbpf.so is located in ../ebpf/libbpf.so
libbpf_path = Path(__file__).parent.parent / "ebpf" / "libbpf.so"
libbpf = ctypes.CDLL(libbpf_path.name, use_errno=True)

class bpf_object(ctypes.Structure):
    pass

class bpf_program(ctypes.Structure):
    pass

class bpf_link(ctypes.Structure):
    pass


# ===== 函数原型 =====

libbpf.bpf_object__open_file.argtypes = [
    ctypes.c_char_p,
    ctypes.c_void_p
]
libbpf.bpf_object__open_file.restype = ctypes.POINTER(bpf_object)

libbpf.bpf_object__load.argtypes = [
    ctypes.POINTER(bpf_object)
]
libbpf.bpf_object__load.restype = ctypes.c_int

libbpf.bpf_object__find_program_by_name.argtypes = [
    ctypes.POINTER(bpf_object),
    ctypes.c_char_p
]
libbpf.bpf_object__find_program_by_name.restype = ctypes.POINTER(bpf_program)

libbpf.bpf_program__fd.argtypes = [
    ctypes.POINTER(bpf_program)
]
libbpf.bpf_program__fd.restype = ctypes.c_int

libbpf.bpf_program__get_type.argtypes = [
    ctypes.POINTER(bpf_program)
]
libbpf.bpf_program__get_type.restype = ctypes.c_int

libbpf.bpf_program__attach_tracepoint.argtypes = [
    ctypes.POINTER(bpf_program),
    ctypes.c_char_p,
    ctypes.c_char_p
]
libbpf.bpf_program__attach_tracepoint.restype = ctypes.POINTER(bpf_link)

libbpf.bpf_link__destroy.argtypes = [
    ctypes.POINTER(bpf_link)
]
libbpf.bpf_link__destroy.restype = None

libbpf.bpf_object__close.argtypes = [
    ctypes.POINTER(bpf_object)
]
libbpf.bpf_object__close.restype = None


# ===== 主逻辑 =====

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <BPF object file>")
        sys.exit(1)

    bpf_obj_path = sys.argv[1].encode()

    obj = libbpf.bpf_object__open_file(bpf_obj_path, None)
    if not obj:
        raise RuntimeError("Failed to open BPF object")

    err = libbpf.bpf_object__load(obj)
    if err != 0:
        raise RuntimeError(f"Failed to load BPF object: {err}")

    prog = libbpf.bpf_object__find_program_by_name(obj, b"handle_tp")
    if not prog:
        raise RuntimeError("Cannot find BPF program 'handle_tp'")

    print(f"prog fd = {libbpf.bpf_program__fd(prog)}")
    print(f"prog type = {libbpf.bpf_program__get_type(prog)}")

    link = libbpf.bpf_program__attach_tracepoint(
        prog,
        b"syscalls",
        b"sys_enter_write"
    )

    if not link:
        raise RuntimeError("Failed to attach tracepoint")

    print("BPF program loaded. Check trace_pipe for events.")
    print("Press Ctrl-C to exit.")

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass

    libbpf.bpf_link__destroy(link)
    libbpf.bpf_object__close(obj)


if __name__ == "__main__":
    main()
