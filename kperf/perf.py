import os
import ctypes
import ctypes.util

# 定义 perf_event_open 的参数
class perf_event_attr(ctypes.Structure):
    _fields_ = [
        ('type', ctypes.c_uint),
        ('size', ctypes.c_uint),
        ('config', ctypes.c_ulonglong),
        ('sample_period', ctypes.c_ulonglong),
        ('sample_type', ctypes.c_ulonglong),
        # 其他参数可以根据需要扩展
    ]

libc = ctypes.CDLL(ctypes.util.find_library("c"))

def perf_event_open(attr, pid, cpu, group_fd, flags):
    syscall = 298  # perf_event_open 的系统调用号,在某些系统上可能不同
    return libc.syscall(syscall, ctypes.byref(attr), pid, cpu, group_fd, flags)

attr = perf_event_attr()
attr.type = 0  # PERF_TYPE_HARDWARE
attr.config = 0  # PERF_COUNT_HW_CPU_CYCLES

# 调用 perf_event_open
fd = perf_event_open(attr, os.getpid(), -1, -1, 0)
if fd == -1:
    raise OSError("perf_event_open failed")
else:
    print(f"perf_event_open success, fd: {fd}")
