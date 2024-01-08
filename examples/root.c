#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    // 尝试提升权限
    if (setuid(0) != 0) {
        perror("setuid");
        exit(EXIT_FAILURE);
    }

    // 在这里执行需要root权限的代码
    printf("Root privileges obtained!\n");

    return 0;
}
