# kperf

kperf is a ebpf-based profiler for linux kernel and user space program, with a more user-friendly web interface.

> [!NOTE]
> this project is not finished yet

## Install

from PYPI

```bash
pip install kperf
```

or from Ubuntu/Debian APT

```bash
sudo apt install kperf
```

## Quick Start

specify the pid to monitor, the program will stop monitoring util the pid is finished or ctrl+c

```bash
sudo kperf -p <pid>
```

for example

```bash
# program function callchain + kernel function callchain
sudo kperf -p 100
sudo kperf -p $(pidof abc)

# kernel function callchain
sudo kperf -p -100
sudo kperf -p 100 -k
sudo kperf -p 100 --kernel
```

## setup timer

you could use `-t` to setup a timer, the program will stop monitoring when time's up

```bash
# monitor 60s
sudo kperf -p <pid> -t 60
```

> Time is measured in seconds

by the way, you can stop monitoring at anytime with ctrl+c (interrupt)

## multiple pids

if your program would create multiple processes(pids) or you are using mpi, try to use `--cgroup`

suppose your program name is abc

```bash
sudo kperf --cgroup $(pgrep abc)
```

## build from scratch

```bash
git submodule update --init --recursive
```

```bash
make -j
```


## result

the program will generate report.html

ctrl+f search all results, right click to highlight the marker

see the demo gif above

## help

welcome to feedback

## reference

- [linux-tools profiler](https://github.com/zq-david-wang/linux-tools/tree/main/perf/profiler)
- [awesome-profiling](https://github.com/msaroufim/awesome-profiling)
- [cpplinks](https://github.com/MattPD/cpplinks)
- [cpplinks performance.tools](https://github.com/MattPD/cpplinks/blob/master/performance.tools.md)
- [推荐适用于 Linux 的 C++ 工具?(分析器、静态分析等)?](https://www.reddit.com/r/cpp/comments/7kurp6/comment/drhpyfh/?utm_source=share)