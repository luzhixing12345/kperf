
import sys
import os
import re

def time_trans(t: float):
    # s
    t = int(t)
    if t < 60:
        return f"{t} s"

    # min
    if t < 60 * 60:
        return f"{t // 60} min {t % 60} s"

    # hour
    return f"{t // 3600} h {t % 3600 // 60} min {t % 60} s"

def clear_lines(num_lines=5):
    # 清空指定行数
    for _ in range(num_lines):
        # 移动光标到当前行的开头
        sys.stdout.write("\033[F")  # 移动光标到上一行
        # 清除当前行
        sys.stdout.write("\033[K")  # 清除当前行
    # 重新定位光标到清空后的行
    sys.stdout.write("\033[F")  # 移动光标到最后一行

def get_numa_node_count():
    node_dir = "/sys/devices/system/node/"
    if not os.path.exists(node_dir):
        return 0
    nodes = [d for d in os.listdir(node_dir) if re.match(r'node\d+', d)]
    return len(nodes)
