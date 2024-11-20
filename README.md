# kperf

linux kernel function callchain profiler

> main source code comes from [linux-tools profiler](https://github.com/zq-david-wang/linux-tools/tree/main/perf/profiler)

![demo](./demo.gif)

## compile

```bash
make 
```

and you will get `src/kperf`

## usage

this program needs to read kernel symbols, so you should always use **sudo**

specify the pid to monitor, the program will stop monitoring util the pid is finished or ctrl+c

```bash
sudo ./src/kperf -p <pid>
```

- pid > 0 means collect **program and kernel** function callchain 
- pid < 0 means collect **kernel** function callchain only, or use `-k`

for example

```bash
# program function callchain + kernel function callchain
sudo ./src/kperf -p 100
sudo ./src/kperf -p $(pidof abc)

# kernel function callchain
sudo ./src/kperf -p -100
sudo ./src/kperf -p 100 -k
sudo ./src/kperf -p 100 --kernel
```

## setup timer

you could use `-t` to setup a timer, the program will stop monitoring when time's up

```bash
# monitor 60s
sudo ./src/kperf -p <pid> -t 60
```

> Time is measured in seconds

by the way, you can stop monitoring at anytime with ctrl+c (interrupt)

## multiple pids

if your program would create multiple processes(pids) or you are using mpi, try to use `--cgroup`

suppose your program name is abc

```bash
sudo ./src/kperf --cgroup $(pgrep abc)
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