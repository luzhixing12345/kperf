import os
import psutil
import re
import time
import json


class Proc:
    def __init__(self, pid):
        self.pid = pid
        self.sleep_time = 0.1

        self.status_path = f"/proc/{pid}/status"
        self.mem_status_list: list[dict[str, float]] = []
        self.dump()

    def dump(self):
        print("pid: %d" % self.pid)

    def is_runing(self):
        try:
            proc = psutil.Process(self.pid)
            # 检查进程是否还在运行
            return proc.is_running() and proc.status() != psutil.STATUS_ZOMBIE
        except psutil.NoSuchProcess:
            return False  # 进程不存在

    def monitor(self):
        while self.is_runing():
            self.mem_status_list.append(self.get_status())
            time.sleep(self.sleep_time)

    def get_status(self):
        """
        https://docs.kernel.org/filesystems/proc.html
        """
        try:
            with open(self.status_path, "r") as f:
                content = f.readlines()
        except ProcessLookupError:
            return {}

        status = {}
        for line in content:
            if line.startswith("VmSize"):
                status["VmSize"] = int(line.split()[1])  # 虚拟内存
            elif line.startswith("VmRSS"):
                status["VmRSS"] = int(line.split()[1])  # 物理内存
            elif line.startswith("RssAnon"):
                status["RssAnon"] = int(line.split()[1])  # 匿名页
            elif line.startswith("RssFile"):
                status["RssFile"] = int(line.split()[1])  # 文件页
        # status['VmSize'] = int(content[17].split()[1]) # 虚拟内存
        # status['VmRSS'] = int(content[21].split()[1]) # 物理内存
        # status['RssAnon'] = int(content[22].split()[1]) # 匿名页
        # status['RssFile'] = int(content[23].split()[1]) # 文件页
        # status['RssShmem'] = int(content[24].split()[1]) # 共享页
        return status

    def save_data(self):
        data_json_path = "data.json"
        # save data as json
        with open(data_json_path, "w") as f:
            json.dump(self.mem_status_list, f)
        print("save data to %s" % data_json_path)
