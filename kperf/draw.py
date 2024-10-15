
import sys
import os
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '.')))

import paperplotlib as ppl
from .proc import Proc

def draw(mem_status_list: list[dict[str, float]]):
  
    if len(mem_status_list) < 10:
        print("data too short")
        return
    else:
        print("data length: ", len(mem_status_list))
    
    graph = ppl.LineGraph()
    
    line_names = ["VmSize", "RSS", "RssAnon", "RssFile"]
    data = []
    vm_size_list = []
    rss_anon_list = []
    rss_file_list = []
    rss = []
    for mem_status in mem_status_list:
        if mem_status == {}:
            continue
        vm_size_list.append(mem_status['VmSize'])
        rss.append(mem_status['VmRSS'])
        rss_anon_list.append(mem_status['RssAnon'])
        rss_file_list.append(mem_status['RssFile'])
    data = [vm_size_list, rss, rss_anon_list, rss_file_list]
    
    graph.disable_x_ticks = True
    graph.disable_points = True
    graph.plot_2d(None, data, line_names)
    graph.save()