
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {

    FILE *fp = fopen("/dev/hello_world", "r");
    char buffer[256];

    if (fp == NULL) {
        printf("Failed to open device file\n");
        return 1;
    }

    srand(time(NULL));

    while (1) {
        int random_num = rand() % 100; // Generate a random number between 0 and 99
        fwrite(&random_num, sizeof(int), 1, fp);
        printf("Sent random number: %d\n", random_num);
        sleep(3);
    }

    return 0;
}
