import os
import psutil
import threading
import time
import json
import sys
from .utils import *
from prettytable import PrettyTable


class Proc:
    def __init__(self, pid):
        self.pid = pid
        self.sleep_time = 0.1

        self.status_path = f"/proc/{pid}/status"
        self.numa_status_path = f"/proc/{pid}/numa_maps"
        self.numa_output_pattern = re.compile(
            r"(?P<address>[0-9a-f]+)\s+\w+\s+(?P<page_type>\w+)=\d+\s+dirty=\d+\s+active=\d+\s+(?P<node_pages>(N\d+=\d+\s*)+)\s+kernelpagesize_kB=\d+"
        )
        self.statistic_infos: list[dict[str, float]] = []
        self.dump()

        # 启动计时器线程
        self.timer_runing = True
        self.timer_thread = threading.Thread(target=self.timer)
        self.timer_thread.daemon = True
        self.timer_thread.start()

    def dump(self):
        print("pid: %d" % self.pid)

    def timer(self):
        start_time = time.time()
        table = PrettyTable()
        table.field_names = ["Time", "Data Count"]
        while self.timer_runing:
            elapsed_time = time.time() - start_time

            table.add_row([time_trans(elapsed_time), len(self.statistic_infos)])
            print(table)
            print("Use Ctrl+C to exit")
            time.sleep(1)  # 每隔1秒更新一次
            clear_lines(5)
            table.clear_rows()

    def is_runing(self):
        try:
            proc = psutil.Process(self.pid)
            # 检查进程是否还在运行
            return proc.is_running() and proc.status() != psutil.STATUS_ZOMBIE
        except psutil.NoSuchProcess:
            return False  # 进程不存在

    def monitor(self):
        try:
            while self.is_runing():
                self.statistic_infos.append(self.get_static_info())
                time.sleep(self.sleep_time)
        except KeyboardInterrupt:
            # 子进程会随着父进程退出而自动退出, 不需要手动 Kill
            ...
        finally:
            self.timer_runing = False
            self.timer_thread.join()

    def get_static_info(self):
        """
        https://docs.kernel.org/filesystems/proc.html

        https://stackoverflow.com/questions/62301373/what-is-the-reason-for-the-discrepancy-between-proc-pid-statusrssanon-and-p

        https://stackoverflow.com/questions/30744333/linux-os-proc-pid-smaps-vs-proc-pid-statm/30799817#30799817
        """
        statistic_info = {
            # 'VmSize': 0,
            # 'VmRSS': 0,
            # 'RssAnon': 0,
            # 'RssFile': 0,
            # 'Numa': {
            #     '0': {
            #         'anon': 0,
            #         'file': 0
            #     }
            # }
        }
        self.get_mem_status(statistic_info)
        self.get_numa_status(statistic_info)
        return statistic_info

    def get_mem_status(self, statistic_info: dict[str, float]):
        try:
            with open(self.status_path, "r") as f:
                content = f.readlines()
        except ProcessLookupError:
            return

        for line in content:
            if line.startswith("VmSize"):
                statistic_info["VmSize"] = int(line.split()[1])  # 虚拟内存
            elif line.startswith("VmRSS"):
                statistic_info["VmRSS"] = int(line.split()[1])  # 物理内存
            elif line.startswith("RssAnon"):
                statistic_info["RssAnon"] = int(line.split()[1])  # 匿名页
            elif line.startswith("RssFile"):
                statistic_info["RssFile"] = int(line.split()[1])  # 文件页

    def get_numa_status(self, statistic_info: dict[str, float]):
        """
        00400000 default file=/a mapped=1361 active=0 N0=1332 N1=29 kernelpagesize_kB=4
        7e3e69cb6000 default anon=59 dirty=59 active=0 N0=54 N1=5 kernelpagesize_kB=4
        """
        # 获取系统 numa 节点数
        numa_num = get_numa_node_count()
        if numa_num == 0:
            return

        try:
            with open(self.numa_status_path, "r") as f:
                content = f.readlines()
        except ProcessLookupError:
            return
        except PermissionError:
            # print(f"no permission to read {self.numa_status_path}")
            return

        statistic_info["Numa"] = {}
        for i in range(numa_num):
            statistic_info["Numa"][str(i)] = {"anon": 0, "file": 0}

        for line in content:
            match = self.numa_output_pattern.match(line)
            if match:
                address = match.group("address")  # 获取内存地址
                page_type = match.group("page_type")  # 获取页的类型 (anon 或者 file)
                node_pages_str = match.group("node_pages")  # 获取节点的页面信息

                # 解析 NUMA 节点的页面数
                node_pages = re.findall(r"N(\d+)=(\d+)", node_pages_str)
                for node, pages in node_pages:
                    statistic_info["Numa"][str(node)][page_type] += int(pages)

    def save_data(self):
        data_json_path = "data.json"
        # save data as json
        with open(data_json_path, "w") as f:
            json.dump(self.statistic_infos, f)
        print("save data to %s" % data_json_path)
