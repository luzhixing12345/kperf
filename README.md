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

```bash
sudo kperf -- ./a.out
sudo kperf -p `pidof a.out`
```

## setup timer

you could use `-t` to setup a timer, the program will stop monitoring when time's up

```bash
# monitor 60s
sudo kperf -p <pid> -t 60
```

> Time is measured in seconds

by the way, you can stop monitoring at anytime with ctrl+c (interrupt)

## Build from scratch

this project need some dependencies

- clang & libbpf-dev: for compiling ebpf code
- libelf-dev: for parsing elf file
- libz-dev: for compressing data
- binutils-dev: for parsing dwarf info

```bash
sudo apt install libbpf-dev libelf-dev clang libz-dev binutils-dev
```

```bash
make
```

## Test

```bash
make
make test
sudo python3 -m unittest discover -s test
```

## reference

- https://www.shadcn.io/
- [linux-tools profiler](https://github.com/zq-david-wang/linux-tools/tree/main/perf/profiler)
- [awesome-profiling](https://github.com/msaroufim/awesome-profiling)
- [cpplinks](https://github.com/MattPD/cpplinks)
- [cpplinks performance.tools](https://github.com/MattPD/cpplinks/blob/master/performance.tools.md)
- [推荐适用于 Linux 的 C++ 工具?(分析器、静态分析等)?](https://www.reddit.com/r/cpp/comments/7kurp6/comment/drhpyfh/?utm_source=share)