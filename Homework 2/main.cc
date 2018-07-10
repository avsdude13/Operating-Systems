#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

#define assertsyscall(x) if (x <= 0) {int err = errno; {perror(#x); exit(err);}}

void HandleSignal(int a);

int main (int argc, char* argvp[]) {
	struct sigaction action;
	int exitcode = 0;
	action.sa_handler = HandleSignal;
	action.sa_flags = SA_RESTART;

	assert(sigaction(SIGUSR1, &action, NULL) == 0);
	assert(sigaction(SIGUSR2, &action, NULL) == 0);
	assert(sigaction(SIGILL, &action, NULL) == 0);

	int childpid = fork();

	if (childpid < 0) { //error
    	perror("fork failure");
    	exit(1);
    } else if (childpid == 0) { //in child
    	assertsyscall(printf("Child PID: %d\n", getpid()));
    	assertsyscall(printf("Parent PID: %d\n", getppid()));

		assert(execl("./child", "child", NULL) == 0);
		
		exitcode = WEXITSTATUS(childpid);
    } else { //in parent
    	assertsyscall(waitpid(childpid, 0, 0));
    	assertsyscall(printf("Process %d exited with status: %d\n", getpid(), exitcode));
    }
    return 0;
}

void HandleSignal(int a) {
	if (a == SIGUSR1) {
		assert(printf("Received SIGUSR1\n") >= 0);
	} else if (a == SIGUSR2) {
		assert(printf("Received SIGUSR2\n") >= 0);
	} else if (a == SIGILL) {
		assert(printf("Received SIGILL\n") >= 0);
	}
}