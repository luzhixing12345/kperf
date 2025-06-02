
#include <asm/unistd.h>
#include <elf.h>
#include <fcntl.h>
#include <sys/types.h>
#include <cerrno>
#include "common.h"
#include <stdio.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "proc_file.h"
#include "process.h"

