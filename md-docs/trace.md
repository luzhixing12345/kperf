
# trace

pid 直接测

program, fork 执行, 拿到 pid 测

- `/proc/self/stack`
- `/proc/[pid]/exe`: 一个符号链接, 指向进程的可执行文件.

  值得一提的是即使运行后删除了原先的可执行文件, 也可以通过此方法找到其 exe 将其 copy 恢复

  ```bash
  $ ./a
  pid: 11786

  $ ls -l /proc/11786/exe
  lrwxrwxrwx 1 kamilu kamilu 0 Jan  9 15:56 /proc/11786/exe -> /home/kamilu/kperf/a

  $ rm a
  $ ls -l /proc/11786/exe
  lrwxrwxrwx 1 kamilu kamilu 0 Jan  9 15:56 /proc/11786/exe -> '/home/kamilu/kperf/a (deleted)'

  $ cp /proc/11786/exe a
  ```

  因为 `/proc` 是一个特殊的文件系统, 在 Linux 2.2 及更高版本下,该文件是一个符号链接,其中包含所执行命令的实际路径名.显然,二进制文件被加载到内存中,并 /proc/[pid]/exe 指向内存中二进制文件的内容.

  > [stackexchange unix](https://unix.stackexchange.com/questions/197854/how-does-the-proc-pid-exe-symlink-differ-from-ordinary-symlinks)

- `/proc/[pid]/maps`

  > [/proc/<pid>/maps简要分析](https://www.cnblogs.com/arnoldlu/p/10272466.html)
  > 
  > [understanding linux proc pid maps or proc self maps](https://stackoverflow.com/questions/1401359/understanding-linux-proc-pid-maps-or-proc-self-maps)

## 参考

- [利用Linux Kernel的perf功能和ebpf的辅助,我要写个自己的profiler 之 第一章:callstack的采集以及如何从函数地址定位到函数名](https://www.bilibili.com/video/BV1Ta411G73D/)
- [what is the need of having both system map file and proc kallsyms](https://stackoverflow.com/questions/28936630/what-is-the-need-of-having-both-system-map-file-and-proc-kallsyms)
- [man7 proc.5](https://man7.org/linux/man-pages/man5/proc.5.html)
- [how to understand proc pid stack](https://stackoverflow.com/questions/33429376/how-to-understand-proc-pid-stack)
- [stackexchange unix](https://unix.stackexchange.com/questions/197854/how-does-the-proc-pid-exe-symlink-differ-from-ordinary-symlinks)
- [/proc/<pid>/maps简要分析](https://www.cnblogs.com/arnoldlu/p/10272466.html)
- [understanding linux proc pid maps or proc self maps](https://stackoverflow.com/questions/1401359/understanding-linux-proc-pid-maps-or-proc-self-maps)
- [baeldung proc-id-maps](https://www.baeldung.com/linux/proc-id-maps)