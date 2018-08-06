#include <iostream>
#include <list>
#include <iterator>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#define NUM_SECONDS 20
#define EVER ;;
#define READ 0
#define WRITE 1
#define MAX_BUFF 1024

#define assertsyscall(x, y) if(!((x) y)){int err = errno; \
    fprintf(stderr, "In file %s at line %d: ", __FILE__, __LINE__); \
        perror(#x); exit(err);}

#ifdef EBUG
#   define dmess(a) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << a << endl;

#   define dprint(a) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << (#a) << " = " << a << endl;

#   define dprintt(a,b) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << a << " " << (#b) << " = " \
    << b << endl
#else
#   define dmess(a)
#   define dprint(a)
#   define dprintt(a,b)
#endif

using namespace std;

// http://man7.org/linux/man-pages/man7/signal-safety.7.html

#define WRITES(a) { const char *foo = a; write(1, foo, strlen(foo)); }
#define WRITEI(a) { char buf[10]; assert(eye2eh(a, buf, 10, 10) != -1); WRITES(buf); }

enum STATE { NEW, RUNNING, WAITING, READY, TERMINATED };

struct PCB
{
    STATE state;
    const char *name;     // name of the executable
    int pid;              // process id from fork();
    int ppid;             // parent process id
    int interrupts;       // number of times interrupted
    int switches;         // may be < interrupts
    int started;          // the time this process started
    int child2parent[2];  // pipe sharing from child to parent
    int parent2child[2];  // pipe sharing from parent to child
};

PCB *running;
PCB *idle;
PCB *front;

// http://www.cplusplus.com/reference/list/list/
list<PCB *> processes;

int sys_time;

/*
** Async-safe integer to a string. i is assumed to be positive. The number
** of characters converted is returned; -1 will be returned if bufsize is
** less than one or if the string isn't long enough to hold the entire
** number. Numbers are right justified. The base must be between 2 and 16;
** otherwise the string is filled with spaces and -1 is returned.
*/
int eye2eh(int i, char *buf, int bufsize, int base)
{
    if(bufsize < 1) return(-1);
    buf[bufsize-1] = '\0';
    if(bufsize == 1) return(0);
    if(base < 2 || base > 16)
    {
        for(int j = bufsize-2; j >= 0; j--)
        {
            buf[j] = ' ';
        }
        return(-1);
    }

    int count = 0;
    const char *digits = "0123456789ABCDEF";
    for(int j = bufsize-2; j >= 0; j--)
    {
        if(i == 0)
        {
            buf[j] = ' ';
        }
        else
        {
            buf[j] = digits[i%base];
            i = i/base;
            count++;
        }
    }
    if(i != 0) return(-1);
    return(count);
}

/*
** a signal handler for those signals delivered to this process, but
** not already handled.
*/
void grab(int signum) { WRITEI(signum); WRITES("\n"); }

// c++decl> declare ISV as array 32 of pointer to function(int) returning void
void(*ISV[32])(int) = {
/*        00    01    02    03    04    05    06    07    08    09 */
/*  0 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 10 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 20 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 30 */ grab, grab
};

/*
** stop the running process and index into the ISV to call the ISR
*/
void ISR(int signum)
{
    if(signum != SIGCHLD)
    {
        if(kill(running->pid, SIGSTOP) == -1)
        {
            WRITES("In ISR kill returned: ");
            WRITEI(errno);
            WRITES("\n");
            return;
        }

        WRITES("In ISR stopped: ");
        WRITEI(running->pid);
        WRITES("\n");
        if(running->state == RUNNING)
        {
            running->state = READY;
        }
    }

    ISV[signum](signum);
}

/*
** an overloaded output operator that prints a PCB
*/
ostream& operator <<(ostream &os, struct PCB *pcb)
{
    os << "state:        " << pcb->state << endl;
    os << "name:         " << pcb->name << endl;
    os << "pid:          " << pcb->pid << endl;
    os << "ppid:         " << pcb->ppid << endl;
    os << "interrupts:   " << pcb->interrupts << endl;
    os << "switches:     " << pcb->switches << endl;
    os << "started:      " << pcb->started << endl;
    return(os);
}

/*
** an overloaded output operator that prints a list of PCBs
*/
ostream& operator <<(ostream &os, list<PCB *> which)
{
    list<PCB *>::iterator PCB_iter;
    for(PCB_iter = which.begin(); PCB_iter != which.end(); PCB_iter++)
    {
        os <<(*PCB_iter);
    }
    return(os);
}

/*
**  send signal to process pid every interval for number of times.
*/
void send_signals(int signal, int pid, int interval, int number)
{
    dprintt("at beginning of send_signals", getpid());

    for(int i = 1; i <= number; i++)
    {
        assertsyscall(sleep(interval), == 0);
        dprintt("sending", signal);
        dprintt("to", pid);
        assertsyscall(kill(pid, signal), == 0)
    }

    dmess("at end of send_signals");
}

struct sigaction *create_handler(int signum, void(*handler)(int))
{
    struct sigaction *action = new(struct sigaction);

    action->sa_handler = handler;

/*
**  SA_NOCLDSTOP
**  If  signum  is  SIGCHLD, do not receive notification when
**  child processes stop(i.e., when child processes  receive
**  one of SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU).
*/
    if(signum == SIGCHLD)
    {
        action->sa_flags = SA_NOCLDSTOP | SA_RESTART;
    }
    else
    {
        action->sa_flags =  SA_RESTART;
    }

    sigemptyset(&(action->sa_mask));
    assert(sigaction(signum, action, NULL) == 0);
    return(action);
}

void sigtrap_func(int sig) 
{
    char buffer[MAX_BUFF];
    int len;
    for (int i=0; (unsigned)i < processes.size(); i++) //see which child sent signal
    {
        PCB* front = processes.front();
        processes.pop_front();
        processes.push_back(front);

        if(front->child2parent[READ] > 0) //found child
        {
            len = read(front->child2parent[READ], buffer, MAX_BUFF); //read pipe
            buffer[len] = 0;
            front->child2parent[READ] = 0;

            if (buffer[0] == '1') 
            {
                WRITES("NUM 1\n"); 
                WRITES("System Time"); WRITEI(sys_time); WRITES("\n");
                front->child2parent[READ] = 0;
                break;
            }
            else if (buffer[0] == '2')
            {
                WRITES("NUM 2\n");
                cout << front << "\n";
                front->child2parent[READ] = 0;
                WRITES("\n");
                break;
            }
            else if (buffer[0] == '3')
            {
                WRITES("NUM 3\n");
                for (int i=0; (unsigned)i < processes.size(); i++)
                {
                    PCB* front = processes.front();
                    processes.pop_front();
                    processes.push_back(front);
                    cout << front << "\n";
                }
                front->child2parent[READ] = 0;
                WRITES("\n");
                break;
            }
            else if (buffer[0] == '4')
            {
                WRITES("NUM 4\n");
                for (int i = 1; i<MAX_BUFF; i++) 
                {
                    buffer[i-1] = buffer[i];
                }
                WRITES(buffer);
                WRITES("\n");
                front->child2parent[READ] = 0;
                break;
            }
        }
    }
}

void scheduler(int signum)
{
    WRITES("---- entering scheduler\n");
    assert(signum == SIGALRM);
    sys_time++;

    int process_pid = 0;
    bool found_process = false;
    char buffer[MAX_BUFF];

    if(processes.size() == 0) //no processes in queue
    {
        WRITES("     continuing "); WRITES(idle->name); WRITES("\n");
        WRITES("     pid"); WRITEI(idle->pid); WRITES("\n");
        WRITES("---- leaving scheduler\n\n");
        idle->state = RUNNING;
        running = idle;

        if(kill(running->pid, SIGCONT) == -1)
        {
            WRITES("in scheduler kill error: ");
            WRITEI(errno);
            WRITES("\n");
            return;
        }
    }

    else if(processes.size() > 0)
    {
        for (int i=0; (unsigned)i < processes.size(); i++)
        {
            PCB* front = processes.front();
            processes.pop_front();
            processes.push_back(front);
            
            if(front->state == NEW) //new
            {
                WRITES("     starting "); WRITES(front->name); WRITES("\n");
                WRITES("     pid"); WRITEI(front->pid); WRITES("\n");
                running->interrupts++;

                front->state = RUNNING;
                front->ppid = getpid();
                front->started = sys_time;
                assertsyscall(pipe(front->child2parent), == 0);
                assertsyscall(pipe(front->parent2child), == 0);

                if((process_pid = fork()) == 0) //child
                {
                    assertsyscall(close(front->child2parent[READ]), == 0);
                    assertsyscall(close(front->parent2child[WRITE]), == 0);
                    dup2(front->child2parent[WRITE], 3);
                    dup2(front->parent2child[READ], 4);
                    assert(execl(front->name, front->name, NULL) != -1);
                    front->pid = process_pid;
                    running = front;
                    found_process = true;

                    WRITES("---- leaving scheduler\n\n");
                    break; 
                } 
                else //parent
                {
                    assertsyscall(close(front->child2parent[WRITE]), == 0);
                    assertsyscall(close(front->parent2child[READ]), == 0);

                    front->pid = process_pid;
                    running = front;
                    found_process = true;

                    WRITES("---- leaving scheduler\n\n");
                    break;
                }
            }
        }

        if(!found_process)
        {
            for (int i=0; (unsigned)i < processes.size(); i++)
            {
                PCB* front = processes.front();
                processes.pop_front();
                processes.push_back(front);

                if(front->state == READY) //ready
                {
                    WRITES("     continuing "); WRITES(front->name); WRITES("\n");
                    WRITES("     pid"); WRITEI(front->pid); WRITES("\n");
                    running->interrupts++;
                    if(running != front)
                    {
                        running->switches++;
                    }

                    front->state = RUNNING;
                    assertsyscall(kill(front->pid, SIGCONT), == 0);

                    running = front;
                    found_process = true;

                    WRITES("---- leaving scheduler\n\n");
                    break;
                }
            }
        }

        if(!found_process)
        {
            WRITES("     continuing "); WRITES(idle->name); WRITES("\n");
            WRITES("     pid"); WRITEI(idle->pid); WRITES("\n");
            WRITES("---- leaving scheduler\n\n"); 
            idle->state = RUNNING;
            running = idle;
        }
    }
}

void process_done(int signum)
{
    assert(signum == SIGCHLD);
    WRITES("\n---- entering process_done\n");

    int status, cpid;

    // we know we received a SIGCHLD so don't wait.
    assert(cpid = waitpid(-1, &status, WNOHANG) != -1);

    if(cpid < 0)
    {
        WRITES("cpid < 0\n");
        assertsyscall(kill(0, SIGTERM), == 0);
    }
    else if(cpid == 0)
    {
        // no more children.
        //break;
    }
    else
    {
        running->started = sys_time - running->started;

        WRITES("     process "); WRITES(running->name); WRITES("\n");
        WRITES("     pid"); WRITEI(running->pid); WRITES("\n");
        WRITES("     status "); WRITEI(running->state); WRITES("\n");
        WRITES("     interrupts "); WRITEI(running->interrupts); WRITES("\n"); 
        WRITES("     switches "); WRITEI(running->switches); WRITES("\n");
        WRITES("     run-time "); WRITEI(running->started); WRITES("\n");

        running->state = TERMINATED;
        running = idle;
        assertsyscall(kill(idle->pid, SIGCONT), == 0);
    }

    WRITES("---- leaving process_done\n\n");
}


/*
** set up the "hardware"
*/
void boot()
{
    sys_time = 0;

    ISV[SIGALRM] = scheduler;
    ISV[SIGCHLD] = process_done;
    struct sigaction *alarm = create_handler(SIGALRM, ISR);
    struct sigaction *child = create_handler(SIGCHLD, ISR);

    // start up clock interrupt
    int ret;
    if((ret = fork()) == 0)
    {
        send_signals(SIGALRM, getppid(), 1, NUM_SECONDS);

        // once that's done, cleanup and really kill everything...
        delete(alarm);
        delete(child);
        delete(idle);
        assertsyscall(kill(0, SIGTERM), == 0);
    }

    if(ret < 0)
    {
        perror("fork");
    }
}

void create_process(char *pname)
{
    front = new(PCB);
    front->state = NEW;
    front->name = pname;
    front->ppid = getpid();
    front->interrupts = 0;
    front->switches = 0;
    front->started = sys_time;

    if((front->pid = fork()) == 0)
    {
        pause();
        perror("pause in create_process");
    }

    processes.push_back(front);
}

void create_idle()
{
    idle = new(PCB);
    idle->state = READY;
    idle->name = "IDLE";
    idle->ppid = getpid();
    idle->interrupts = 0;
    idle->switches = 0;
    idle->started = sys_time;

    if((idle->pid = fork()) == 0)
    {
        pause();
        perror("pause in create_idle");
    }
}

int main(int argc, char **argv)
{
    boot();

    signal(SIGTRAP, sigtrap_func);

    create_idle();
    running = idle;
    cout << running << "\n";

    for(int i =1; i < argc; i++)
    {
        create_process(argv[i]);
        running = front;
        cout << running << "\n"; 
    }

    running = idle;

    // we keep this process around so that the children don't die and
    // to keep the IRQs in place.
    for(EVER)
    {
        // "Upon termination of a signal handler started during a
        // pause(), the pause() call will return."
        pause();
    }
}
