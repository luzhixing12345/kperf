import sys
import os

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), ".")))

import paperplotlib as ppl
from .utils import *


def draw(statistic_infos: list[dict[str, float]], output_dir: str):
    if len(statistic_infos) < 10:
        print("too few data, try to use -t <sleep time> to get more data")
        return
    else:
        print("data length: ", len(statistic_infos))

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    draw_file_anon(statistic_infos, output_dir)
    draw_numa(statistic_infos, output_dir)
    draw_numa_anon_file(statistic_infos, output_dir)
    draw_page_types(statistic_infos, output_dir)


def draw_file_anon(statistic_infos: list[dict[str, float]], output_dir: str):
    graph = ppl.LineGraph()

    line_names = ["VmSize", "RSS", "RssAnon", "RssFile"]
    data = []
    vm_size_list = []
    rss_anon_list = []
    rss_file_list = []
    rss = []
    for statistic_info in statistic_infos:
        if statistic_info == {}:
            continue
        vm_size_list.append(statistic_info["VmSize"])
        rss.append(statistic_info["VmRSS"])
        rss_anon_list.append(statistic_info["RssAnon"])
        rss_file_list.append(statistic_info["RssFile"])
    data = [vm_size_list, rss, rss_anon_list, rss_file_list]

    graph.disable_x_ticks = True
    graph.disable_points = True
    graph.plot_2d(None, data, line_names)

    graph.x_label = "Time (s)"
    graph.y_label = "Memory Usage (KB)"
    graph.title = "Anonymous & File pages"
    graph.save(os.path.join(output_dir, "file_anon.png"))


def draw_numa(statistic_infos: list[dict[str, float]], output_dir: str):

    if statistic_infos[0].get("Numa") is None:
        return

    graph = ppl.LineGraph()
    # statistic_info = {
    #         # 'VmSize': 0,
    #         # 'VmRSS': 0,
    #         # 'RssAnon': 0,
    #         # 'RssFile': 0,
    #         # 'Numa': {
    #         #     '0': {
    #         #         'anon': 0,
    #         #         'file': 0
    #         #     }
    #         # }
    #     }
    numa_num = get_numa_node_count()
    line_names = []
    for i in range(numa_num):
        line_names.append("Node" + str(i))

    data = [[] for i in range(numa_num)]
    for statistic_info in statistic_infos:
        if statistic_info == {}:
            continue
        for i in range(numa_num):
            data[i].append(statistic_info["Numa"][str(i)]["anon"] + statistic_info["Numa"][str(i)]["file"])

    graph.disable_x_ticks = True
    graph.disable_points = True
    graph.plot_2d(None, data, line_names)

    graph.x_label = "Time (s)"
    graph.y_label = "Memory Usage (KB)"
    graph.title = "NUMA"
    graph.save(os.path.join(output_dir, "numa.png"))


def draw_numa_anon_file(statistic_infos: list[dict[str, float]], output_dir: str):

    if statistic_infos[0].get("Numa") is None:
        return

    graph = ppl.LineGraph()
    # statistic_info = {
    #         # 'VmSize': 0,
    #         # 'VmRSS': 0,
    #         # 'RssAnon': 0,
    #         # 'RssFile': 0,
    #         # 'Numa': {
    #         #     '0': {
    #         #         'anon': 0,
    #         #         'file': 0
    #         #     }
    #         # }
    #     }
    numa_num = get_numa_node_count()
    line_names = []
    for i in range(numa_num):
        line_names.append("Node" + str(i) + " anon")
        line_names.append("Node" + str(i) + " file")

    data = [[] for i in range(numa_num * 2)]
    for statistic_info in statistic_infos:
        if statistic_info == {}:
            continue
        index = 0
        for i in range(numa_num):
            data[index].append(statistic_info["Numa"][str(i)]["anon"])
            data[index + 1].append(statistic_info["Numa"][str(i)]["file"])
            index += 2

    graph.disable_x_ticks = True
    graph.disable_points = True
    graph.plot_2d(None, data, line_names)

    graph.x_label = "Time (s)"
    graph.y_label = "Memory Usage (KB)"
    graph.title = "NUMA anon & file"
    graph.save(os.path.join(output_dir, "numa_anon_file.png"))

def draw_page_types(statistic_infos: list[dict[str, float]], output_dir: str):

    graph = ppl.LineGraph()
    
    # statistic_info["page_types"] = {"referenced": 0, "dirty": 0, "total": int(total_page_count)}
    
    line_names = ['referenced', 'dirty', 'total']

    data = [[] for i in range(3)]
    for statistic_info in statistic_infos:
        if statistic_info == {}:
            continue
        data[0].append(statistic_info["page_types"]["referenced"])
        data[1].append(statistic_info["page_types"]["dirty"])
        data[2].append(statistic_info["page_types"]["total"])

    graph.disable_x_ticks = True
    graph.disable_points = True
    graph.plot_2d(None, data, line_names)

    graph.x_label = "Time (s)"
    graph.y_label = "Memory Usage (KB)"
    graph.title = "Page Types"
    graph.save(os.path.join(output_dir, "page_types.png"))