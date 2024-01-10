
# proc

proc 文件系统充当内核中内部数据结构的接口.它可用于获取有关系统的信息,并在运行时更改某些内核参数. 首先来看一下 /proc 的只读部分, 然后介绍一下如何使用 /proc/sys 更改设置

## 参考

- [kernel docs proc](https://docs.kernel.org/filesystems/proc.html)