#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define assertsyscall(x) if (x <= 0) {int err = errno; {perror(#x); exit(err);}}

int main(int argc, char *argv[]) {
	int processid;
	int retval;
    long counter = strtol(argv[1], 0, 10);

    assertsyscall((processid = getpid()));

    for (int i = 1; i < counter + 1; i++) {
        printf("Process: %d %d\n", processid, i);
        retval = i;
    }
    return retval;
}
