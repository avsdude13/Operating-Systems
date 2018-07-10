#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

#define assertsyscall(x) if (x <= 0) {int err = errno; {perror(#x); exit(err);}}

int main() {
	pid_t childpid, parentpid;
	int exitcode;

    if((childpid = fork()) < 0) { //error
    	perror("fork failure");
    	exit(1);
    } else if (childpid == 0) { //in child
    	assertsyscall((childpid = getpid()));
    	assertsyscall((parentpid = getppid()));

    	assertsyscall(printf("Child PID: %d\n", childpid));
    	assertsyscall(printf("Parent PID: %d\n", parentpid));

		assertsyscall((execl("./counter", "counter", "5", NULL)));
		
		exitcode = WEXITSTATUS(childpid);
        assertsyscall(printf("Process %d exited with status: %d\n", childpid, exitcode));
    } else { //in parent
    	assertsyscall(waitpid(childpid, 0, 0));
    }
    return 0;
}

/*
	Some of the code for calling the counter class came from
	https://stackoverflow.com/questions/5460421/how-do-you-write-a-c-program-to-execute-another-program
*/
