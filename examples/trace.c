#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

void printStackTrace() {
    void *array[10];
    size_t size;
    char **strings;

    size = backtrace(array, 10);
    strings = backtrace_symbols(array, size);

    printf("Function call stack:\n");
    for (size_t i = 0; i < size; i++) {
        printf("%s\n", strings[i]);
    }

    free(strings);
}

void funcB() {
    printStackTrace();
}

void funcA() {
    funcB();
}

int main() {
    funcA();
    return 0;
}
