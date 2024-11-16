# kperf

linux kernel function callchain profiler

## compile

```bash
make 
```

and you will get `src/kperf`

## usage

> this program needs to read kernel symbols, so you should use **sudo**

```bash
Usage: kperf [-p --pid][-t --timeout][-h --help][-v --version]

Description: kernel callchain profiler


  -p   --pid       pid > 0 to specify a process's function callchain;
                   pid < 0 to specify kernel and user function callchain
  -t   --timeout   maximum time in seconds
  -h   --help      show help information
  -v   --version   show version
```


```bash
sudo ./src/kperf
```

## reference

- [linux-tools profiler](https://github.com/zq-david-wang/linux-tools/tree/main/perf/profiler)
- [awesome-profiling](https://github.com/msaroufim/awesome-profiling)
- [cpplinks](https://github.com/MattPD/cpplinks)
- [cpplinks performance.tools](https://github.com/MattPD/cpplinks/blob/master/performance.tools.md)
- [推荐适用于 Linux 的 C++ 工具?(分析器、静态分析等)?](https://www.reddit.com/r/cpp/comments/7kurp6/comment/drhpyfh/?utm_source=share)- [awesome-profiling](https://github.com/msaroufim/awesome-profiling)
- [cpplinks](https://github.com/MattPD/cpplinks)
- [cpplinks performance.tools](https://github.com/MattPD/cpplinks/blob/master/performance.tools.md)
- [推荐适用于 Linux 的 C++ 工具?(分析器、静态分析等)?](https://www.reddit.com/r/cpp/comments/7kurp6/comment/drhpyfh/?utm_source=share)