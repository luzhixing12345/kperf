
#include "common.h"

#include <stdio.h>

int main(void)
{
    printf("single process, single thread\n");
    run_5s_loop();
    return 0;
}
