
# maps

/proc/[pid]/maps 记录了对应进程的内存映射的实际虚拟地址

创建下面的一个简单测试代码, 输出当前进程的 pid 号之后死循环卡住.

```c
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int pid = getpid();
    printf("pid: %d\n", pid);
    while (1) {
        
    }
    return 0;
}
```

编译执行可以看到当前的 pid

```bash
(base) kamilu@LZX:~/kperf$ ./a
pid: 9347
```

找到 /proc 下对应的进程的 maps 信息, 使用 cat 输出得到一个如下所示的结果

> 每一次的内存映射是不相同的, 可能会有一个偏移量, 这是操作系统内核为了保护程序做的随机起始地址虚拟地址. 除此之外 stack 和 heap 也都会有 random address offset

```bash
(base) kamilu@LZX:~/kperf$ cat /proc/9347/maps
55c5525d1000-55c5525d2000 r--p 00000000 08:20 943769                     /home/kamilu/kperf/a
55c5525d2000-55c5525d3000 r-xp 00001000 08:20 943769                     /home/kamilu/kperf/a
55c5525d3000-55c5525d4000 r--p 00002000 08:20 943769                     /home/kamilu/kperf/a
55c5525d4000-55c5525d5000 r--p 00002000 08:20 943769                     /home/kamilu/kperf/a
55c5525d5000-55c5525d6000 rw-p 00003000 08:20 943769                     /home/kamilu/kperf/a
55c553831000-55c553852000 rw-p 00000000 00:00 0                          [heap]
7f2b9eabf000-7f2b9eac2000 rw-p 00000000 00:00 0
7f2b9eac2000-7f2b9eaea000 r--p 00000000 08:20 36013                      /usr/lib/x86_64-linux-gnu/libc.so.6
7f2b9eaea000-7f2b9ec7f000 r-xp 00028000 08:20 36013                      /usr/lib/x86_64-linux-gnu/libc.so.6
7f2b9ec7f000-7f2b9ecd7000 r--p 001bd000 08:20 36013                      /usr/lib/x86_64-linux-gnu/libc.so.6
7f2b9ecd7000-7f2b9ecdb000 r--p 00214000 08:20 36013                      /usr/lib/x86_64-linux-gnu/libc.so.6
7f2b9ecdb000-7f2b9ecdd000 rw-p 00218000 08:20 36013                      /usr/lib/x86_64-linux-gnu/libc.so.6
7f2b9ecdd000-7f2b9ecea000 rw-p 00000000 00:00 0
7f2b9ed01000-7f2b9ed03000 rw-p 00000000 00:00 0
7f2b9ed03000-7f2b9ed05000 r--p 00000000 08:20 35916                      /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
7f2b9ed05000-7f2b9ed2f000 r-xp 00002000 08:20 35916                      /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
7f2b9ed2f000-7f2b9ed3a000 r--p 0002c000 08:20 35916                      /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
7f2b9ed3b000-7f2b9ed3d000 r--p 00037000 08:20 35916                      /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
7f2b9ed3d000-7f2b9ed3f000 rw-p 00039000 08:20 35916                      /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
7ffc68c20000-7ffc68c42000 rw-p 00000000 00:00 0                          [stack]
7ffc68d2d000-7ffc68d31000 r--p 00000000 00:00 0                          [vvar]
7ffc68d31000-7ffc68d33000 r-xp 00000000 00:00 0                          [vdso]
```

其中第一个是我们的程序, 后面是加载到更高地址的共享动态库. 每一行的输出含义如下所示

```bash
<start addr>-<end addr>    <mode>  <offset> <major id:minor id>   <inode id>   <file path> 

55c5525d1000-55c5525d2000  r--p    00000000        08:20          943769       /home/kamilu/kperf/a
```

ELF 文件包含不同类型的段, 多个目标文件链接成最终的可执行文件时会将相同类型的段进行合并. 进程地址空间从**低地址向高地址**依次是代码段(Text)/数据段(Data)/BSS段/堆/内存映射段(mmap)/栈, 如下图所示

![20240110174114](https://raw.githubusercontent.com/learner-lu/picbed/master/20240110174114.png)

可以将 mode 中具有 `x` 执行权限的段解析出来, 根据对应的格式找到

> 这个程序比较简单只使用了基本 libc 和 ld, 比较复杂的可能还会用到类似 libpthread.so libgcc_s.so libm.so libstdc++.so libdl.so, 或者第三方动态库, 不过原理相同

## 参考

- [baeldung proc-id-maps](https://www.baeldung.com/linux/proc-id-maps)