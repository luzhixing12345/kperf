
# proc

proc 文件系统充当内核中内部数据结构的接口.它可用于获取有关系统的信息,并在运行时更改某些内核参数. 首先来看一下 /proc 的只读部分, 然后介绍一下如何使用 /proc/sys 更改设置

## 参考

- [kernel docs proc](https://docs.kernel.org/filesystems/proc.html)
- [/proc/vmstat输出含义](https://blog.csdn.net/kaka__55/article/details/125236633)
- [gitee data-profile-tools](https://gitee.com/anolis/data-profile-tools)
- [检查进程页表 (翻译 by chatgpt)](https://www.cnblogs.com/pengdonglin137/p/17876240.html)
- [内存不超过5M,datop 在识别冷热内存及跨 numa 访存有多硬核?| 龙蜥技术](https://segmentfault.com/a/1190000041792423)