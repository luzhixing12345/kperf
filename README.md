# kperf

kperf is a simple perf with eBPF and friendly-web interface

> [!NOTE]
> this project is not finished yet

## Install

```bash
sudo add-apt-repository ppa:kamilu/kperf
sudo apt install kperf
```

## Quick Start

```bash
sudo kperf -- ./a.out
sudo kperf -p `pidof a.out`
```

## Build from scratch

this project need some dependencies

- clang & libbpf-dev: for compiling ebpf code
- libelf-dev: for parsing elf file
- libz-dev: for compressing data
- libiberty-dev: for name demangling for cpp/rust elf file
- libdw-dev: for parsing dwarf
- libdwarf-dev: for parsing dwarf

```bash
sudo apt install libbpf-dev libelf-dev clang libz-dev libiberty-dev libdw-dev libdwarf-dev libzstd-dev
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

## Build deb

```bash
./scripst/build.sh
```

publish to launchpad.net

```bash
debuild -S -sa
dput ppa:kamilu/kperf <source.changes>
```

see https://launchpad.net/~kamilu/+archive/ubuntu/kperf/+builds?build_state=built

## reference

- UI: https://www.shadcn.io/
- [Linux内核 eBPF基础：perf（4）perf_event_open系统调用与用户手册详解](https://blog.csdn.net/Rong_Toa/article/details/117040529)
- [linux-tools profiler](https://github.com/zq-david-wang/linux-tools/tree/main/perf/profiler)
- [awesome-profiling](https://github.com/msaroufim/awesome-profiling)
- [cpplinks](https://github.com/MattPD/cpplinks)
- [cpplinks performance.tools](https://github.com/MattPD/cpplinks/blob/master/performance.tools.md)
- [推荐适用于 Linux 的 C++ 工具?(分析器、静态分析等)?](https://www.reddit.com/r/cpp/comments/7kurp6/comment/drhpyfh/?utm_source=share)
- [深入浅出Debian包制作：从原理到实践深入浅出Debian包制作：从原理到实践](https://juejin.cn/post/7591304659131383846)

### ebpf

- [深入理解基于 eBPF 的 C/C++ 内存泄漏分析](https://zhuanlan.zhihu.com/p/665778795)
- [利用 eBPF BCC 无侵入分析服务函数耗时](https://selfboot.cn/2023/06/30/ebpf_func_time/)
- [anquanke member](https://www.anquanke.com/member.html?memberId=155655)
- [BPF相关(1) 工具链libbpf入门指南](https://zhuanlan.zhihu.com/p/615573175)

### deb build

- [手把手教你如何在linux环境下制作deb软件包](https://blog.csdn.net/qq_43257914/article/details/131609720)
- [深入浅出Debian包制作：从原理到实践深入浅出Debian包制作：从原理到实践 前言 在Linux世界中，软件分发有多](https://juejin.cn/post/7591304659131383846)
- [shell debmaker](https://github.com/kellanfan/shell/tree/master/projects/debmaker)
- [将软件包上传到 PPA](https://documentation.ubuntu.com/launchpad/user/how-to/packaging/ppa-package-upload/index.html)
- [askubuntu upload-to-ppa-succeeded-but-packages-doesnt-appear](https://askubuntu.com/questions/26129/upload-to-ppa-succeeded-but-packages-doesnt-appear)