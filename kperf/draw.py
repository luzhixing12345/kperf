import sys
import os

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), ".")))

import paperplotlib as ppl
from .utils import *


def draw(statistic_infos: list[dict[str, float]]):
    if len(statistic_infos) < 10:
        print("too few data, try to use -t <sleep time> to get more data")
        return
    else:
        print("data length: ", len(statistic_infos))

    draw_file_anon(statistic_infos)
    draw_numa(statistic_infos)
    draw_numa_anon_file(statistic_infos)


def draw_file_anon(statistic_infos: list[dict[str, float]]):
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
    graph.save("anon_file.png")


def draw_numa(statistic_infos: list[dict[str, float]]):

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
    graph.save("numa.png")


def draw_numa_anon_file(statistic_infos: list[dict[str, float]]):

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
    graph.save("numa_anon_file.png")
