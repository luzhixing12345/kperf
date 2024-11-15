
#pragma once

#include <asm/unistd.h>
#include <elf.h>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

//--------------------------------symbols-------------------------------------------
using STORE_T = std::map<unsigned long long, std::pair<std::string, unsigned long long>>;
using K_STORE_T = std::map<unsigned long long, std::string>;

K_STORE_T *load_kernel();
STORE_T *load_symbol_pid(int pid);