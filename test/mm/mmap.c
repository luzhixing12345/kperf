#define MAXN 1024

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

struct {
    void *addr;
    size_t n;
} maps[MAXN];

int main() {
    int i, n, k, r;
    void *p;
    for (i = 0; i < MAXN; i++) {
        n = 1024 * ((rand() % 32) + 1);
        p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) {
            perror("fail to mmap");
            return -1;
        }
        maps[i].addr = p;
        maps[i].n = n;
    }
    for (i = 0; i < 10000000; i++) {
        k = rand() % MAXN;
        r = munmap(maps[k].addr, maps[k].n);
        if (r) {
            perror("fail to munmap");
            return -1;
        }
        n = 1024 * ((rand() % 32) + 1);
        p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) {
            perror("fail to mmap");
            return -1;
        }
        maps[k].addr = p;
        maps[k].n = n;
    }
    for (i = 0; i < MAXN; i++) munmap(maps[i].addr, maps[i].n);
    return 0;
}