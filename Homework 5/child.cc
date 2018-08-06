#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <errno.h>
#include <string>

extern int errno;

#define assertsyscall(x, y) if(!((x) y)){int err = errno; \
    fprintf(stderr, "In file %s at line %d: ", __FILE__, __LINE__); \
        perror(#x); exit(err);}

#define READ 4
#define WRITE 3

int main (int argc, char** argv) {
    // char *val = "1";
    // char *val = "2";
    // char *val = "3";
    char *val = "4Hello World";

    assertsyscall(write(3, val, strlen(val)), != -1);
    kill(getppid(), SIGTRAP);
}