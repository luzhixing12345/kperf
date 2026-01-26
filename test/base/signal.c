
#include <stdio.h>
#include <signal.h>
#include <string.h>

void handler(int sig) {
    printf("Caught signal %d (%s)\n", sig, strsignal(sig));
}

int main(int argc, char **argv) {
    signal(SIGINT, handler);
    signal(SIGUSR1, handler);
    signal(SIGTERM, handler);
    printf("Enter signal\n");
    raise(SIGINT);
    raise(SIGUSR1);
    while (1) {
        
    }
    return 0;
}
