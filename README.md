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

<!-- - CPU：哪些线程在工作？这些线程都在干嘛？这些线程各自消耗了多少 CPU Time？
- 内存：当前内存中存储了哪些东西？这些东西的命中率情况？（通常我们更关注业务缓存）？
- 网络 I/O：QPS/TPS 有异常吗？当前主要的网络 I/O 是由什么请求发起的？带宽还够吗？请求延迟？长链接还是短链接（衡量 syscall 的开销）？
- 磁盘 I/O：磁盘在读写文件吗？读写哪些文件？大多数的读写是什么 Pattern？吞吐多大？一次 I/O 延迟多大？ -->

## Build from scratch

this project need some dependencies

- clang & libbpf-dev: for compiling ebpf code
- libelf-dev: for parsing elf file
- libz-dev: for compressing data
- libiberty-dev: for name demangling for cpp/rust elf file
- libdw-dev: for parsing dwarf

```bash
sudo apt install libbpf-dev libelf-dev clang libz-dev libdw-dev libiberty-dev
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
- [perf-prof](https://github.com/OpenCloudOS/perf-prof)
- [Linux内核 eBPF基础：perf（4）perf_event_open系统调用与用户手册详解](https://blog.csdn.net/Rong_Toa/article/details/117040529)
- [linux-tools profiler](https://github.com/zq-david-wang/linux-tools/tree/main/perf/profiler)
- [awesome-profiling](https://github.com/msaroufim/awesome-profiling)
- [cpplinks](https://github.com/MattPD/cpplinks)
- [cpplinks performance.tools](https://github.com/MattPD/cpplinks/blob/master/performance.tools.md)
- [推荐适用于 Linux 的 C++ 工具?(分析器、静态分析等)?](https://www.reddit.com/r/cpp/comments/7kurp6/comment/drhpyfh/?utm_source=share)
- [深入浅出Debian包制作：从原理到实践深入浅出Debian包制作：从原理到实践](https://juejin.cn/post/7591304659131383846)

### ebpf

- [eBPF-libbpf学习路径分享](https://www.bilibili.com/video/BV1xt421W7vp)
- [BPF之路一bpf系统调用](https://www.anquanke.com/post/id/263803)
- [深入理解基于 eBPF 的 C/C++ 内存泄漏分析](https://zhuanlan.zhihu.com/p/665778795)
- [利用 eBPF BCC 无侵入分析服务函数耗时](https://selfboot.cn/2023/06/30/ebpf_func_time/)
- [anquanke member](https://www.anquanke.com/member.html?memberId=155655)
- [BPF相关(1) 工具链libbpf入门指南](https://zhuanlan.zhihu.com/p/615573175)
- [eBPF学习实践系列（一） -- 初识eBPF](https://xiaodongq.github.io/2024/06/06/ebpf_learn/)
- [我眼中的分布式系统可观测性 TiDB key vis](https://tidb.net/blog/758968d9)

### deb build

- [手把手教你如何在linux环境下制作deb软件包](https://blog.csdn.net/qq_43257914/article/details/131609720)
- [深入浅出Debian包制作：从原理到实践深入浅出Debian包制作：从原理到实践 前言 在Linux世界中，软件分发有多](https://juejin.cn/post/7591304659131383846)
- [shell debmaker](https://github.com/kellanfan/shell/tree/master/projects/debmaker)
- [将软件包上传到 PPA](https://documentation.ubuntu.com/launchpad/user/how-to/packaging/ppa-package-upload/index.html)
- [askubuntu upload-to-ppa-succeeded-but-packages-doesnt-appear](https://askubuntu.com/questions/26129/upload-to-ppa-succeeded-but-packages-doesnt-appear)