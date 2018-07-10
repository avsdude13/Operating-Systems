#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

int main(int argc, char* argv[]) {
    pid_t parent = getppid();
    kill(parent, SIGUSR1);
    printf("Sending SIGUSR1\n");
    kill(parent, SIGUSR1);
    printf("Sending SIGUSR1\n");
    kill(parent, SIGUSR1);
    printf("Sending SIGUSR1\n");

    kill(parent, SIGUSR2);
    printf("Sending SIGUSR2\n");
    kill(parent, SIGILL);
    printf("Sending SIGILL\n");
}