#include <iostream>
#include <list>
#include <iterator>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <vector>
#include <sstream>

/*
colaberated with the cs room on friday

g++ -std=c++11  CPU.cc

This program does the following.
1) Create handlers for two signals.
2) Create an idle process which will be executed when there is nothing
   else to do.
3) Create a send_signals process that sends a SIGALRM every so often

When run, it should produce the following output (approximately):

$ ./a.out
in CPU.cc at 247 main pid = 26428
state:    1
name:     IDLE
pid:      26430
ppid:     0
slices:   0
switches: 0
started:  0
in CPU.cc at 100 at beginning of send_signals getpid () = 26429
in CPU.cc at 216 idle getpid () = 26430
in CPU.cc at 222 going to sleep
in CPU.cc at 106 sending signal = 14
in CPU.cc at 107 to pid = 26428
in CPU.cc at 148 stopped running->pid = 26430
in CPU.cc at 155 continuing tocont->pid = 26430
in CPU.cc at 106 sending signal = 14
in CPU.cc at 107 to pid = 26428
in CPU.cc at 148 stopped running->pid = 26430
in CPU.cc at 155 continuing tocont->pid = 26430
in CPU.cc at 106 sending signal = 14
in CPU.cc at 107 to pid = 26428
in CPU.cc at 115 at end of send_signals
Terminated
---------------------------------------------------------------------------
Add the following functionality.
1) Change the NUM_SECONDS to 20.

2) Take any number of arguments for executables, and place each on new_list.
    The executable will not require arguments themselves.

3) When a SIGALRM arrives, scheduler() will be called. It calls
    choose_process which currently always returns the idle process. Do the
    following.
    a) Update the PCB for the process that was interrupted including the
        number of context switches and interrupts it had, and changing its
        state from RUNNING to READY.
    b) If there are any processes on the new_list, do the following.
        i) Take the one off the new_list and put it on the processes list.
        ii) Change its state to RUNNING, and fork() and execl() it.
    c) Modify choose_process to round robin the processes in the processes
        queue that are READY. If no process is READY in the queue, execute
        the idle process.

4) When a SIGCHLD arrives notifying that a child has exited, process_done() is
    called. process_done() currently only prints out the PID and the status.
    a) Add the printing of the information in the PCB including the number
        of times it was interrupted, the number of times it was context
        switched (this may be fewer than the interrupts if a process
        becomes the only non-idle process in the ready queue), and the total
        system time the process took.
    b) Change the state to TERMINATED.
    c) Start the idle process to use the rest of the time slice.
*/

#define NUM_SECONDS 20

// make sure the asserts work
#undef NDEBUG
#include <assert.h>

#define EBUG
#ifdef EBUG
#   define dmess(a) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << a << endl;

#   define dprint(a) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << (#a) << " = " << a << endl;

#   define dprintt(a,b) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << a << " " << (#b) << " = " \
    << b << endl
#else
#   define dprint(a)
#endif /* EBUG */

//***************************ADDED***************************************
#define READ_END 0
#define WRITE_END 1

#define NUM_CHILDREN 5
#define NUM_PIPES 2

#define P2K 0
#define K2P 1

#define WRITE(a) { const char *foo = a; write (1, foo, strlen (foo)); }

int child_count = 0;
//************************************************************************

using namespace std;

enum STATE { NEW, RUNNING, WAITING, READY, TERMINATED };

/*
** a signal handler for those signals delivered to this process, but
** not already handled.
*/
void grab (int signum) { dprint (signum); }

// c++decl> declare ISV as array 32 of pointer to function (int) returning
// void
void (*ISV[32])(int) = {
/*        00    01    02    03    04    05    06    07    08    09 */
/*  0 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 10 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 20 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 30 */ grab, grab
};

struct PCB
{
    STATE state;
    const char *name;   // name of the executable
    int pid;            // process id from fork();
    int ppid;           // parent process id
    int interrupts;     // number of times interrupted
    int switches;       // may be < interrupts
    int started;        // the time this process started
//********************************************************
    int pipes[NUM_PIPES][2];        // the pipe
};

/*
** an overloaded output operator that prints a PCB
*/
ostream& operator << (ostream &os, struct PCB *pcb)
{
    os << "state:        " << pcb->state << endl;
    os << "name:         " << pcb->name << endl;
    os << "pid:          " << pcb->pid << endl;
    os << "ppid:         " << pcb->ppid << endl;
    os << "interrupts:   " << pcb->interrupts << endl;
    os << "switches:     " << pcb->switches << endl;
    os << "started:      " << pcb->started << endl;
    return (os);
}

/*
** an overloaded output operator that prints a list of PCBs
*/
ostream& operator << (ostream &os, list<PCB *> which)
{
    list<PCB *>::iterator PCB_iter;
    for (PCB_iter = which.begin(); PCB_iter != which.end(); PCB_iter++)
    {
        os << (*PCB_iter);
    }
    return (os);
}

PCB *running;
PCB *idle;

// http://www.cplusplus.com/reference/list/list/
list<PCB *> new_list;
list<PCB *> processes;

int sys_time;

/*
** Async-safe integer to a string. i is assumed to be positive. The number
** of characters converted is returned; -1 will be returned if bufsize is
** less than one or if the string isn't long enough to hold the entire
** number. Numbers are right justified. The base must be between 2 and 16;
** otherwise the string is filled with spaces and -1 is returned.
*/
int eye2eh (int i, char *buf, int bufsize, int base)
{
    if (bufsize < 1) return (-1);
    buf[bufsize-1] = '\0';
    if (bufsize == 1) return (0);
    if (base < 2 || base > 16)
    {
        for (int j = bufsize-2; j >= 0; j--)
        {
            buf[j] = ' ';
        }
        return (-1);
    }

    int count = 0;
    const char *digits = "0123456789ABCDEF";
    for (int j = bufsize-2; j >= 0; j--)
    {
        if (i == 0)
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
    if (i != 0) return (-1);
    return (count);
}
//********************************************************************

/*
**  send signal to process pid every interval for number of times.
*/
void send_signals (int signal, int pid, int interval, int number)
{
    dprintt ("at beginning of send_signals", getpid ());

    for (int i = 1; i <= number; i++)
    {
        sleep (interval);

        dprintt ("sending", signal);
        dprintt ("to", pid);

        if (kill (pid, signal) == -1)
        {
            perror ("kill");
            return;
        }
    }
    dmess ("at end of send_signals");
}

struct sigaction *create_handler (int signum, void (*handler)(int))
{
    struct sigaction *action = new (struct sigaction);

    action->sa_handler = handler;
/*
**  SA_NOCLDSTOP
**  If  signum  is  SIGCHLD, do not receive notification when
**  child processes stop (i.e., when child processes  receive
**  one of SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU).
*/
    if (signum == SIGCHLD)
    {
        action->sa_flags = SA_NOCLDSTOP;
    }
    else
    {
        action->sa_flags = 0;
    }

    sigemptyset (&(action->sa_mask));

    assert (sigaction (signum, action, NULL) == 0);
    return (action);
}

PCB* choose_process ()
{
    running->state = READY;
    
    running->interrupts++;

    if(!new_list.empty()) 
    {
        //cout << "new list pull" << endl;
        PCB *pcb = new_list.front();
        
        pid_t pid = fork();
        
        if(pid == 0) {

            close (pcb->pipes[P2K][READ_END]);
            
            close (pcb->pipes[K2P][WRITE_END]);

            // assign fildes 3 and 4 to the pipe ends in the child
            
            dup2 (pcb->pipes[P2K][WRITE_END], 3);
            
            dup2 (pcb->pipes[K2P][READ_END], 4);

            //cout << "im execl" << endl;
            execl(pcb->name, pcb->name, NULL);
        }
        
        else if(pid < 0)
        {
            perror("pid fork fail");
            
        } else {
            pcb->state = RUNNING;
            
            pcb->pid = pid;
            
            pcb->ppid = getpid();
            
            running->switches++;
            
            pcb->started = sys_time;
            
            processes.push_back(pcb);
            
            new_list.pop_front();
            
            running = processes.back();
            
            //return running;
        }
        
    } else {
    
        list<PCB *> :: iterator it = processes.begin();
        
        for(; it != processes.end(); it++) 
        {
            if((*it)->state == READY) 
            {
                if((*it)->pid != running->pid) 
                {
                    running->switches++;
                }
                
                processes.erase(it);
                
                processes.push_back(*it);
                
                running = *it;
                
                running->state = RUNNING;
            }
            
            else
                running = idle;
        }
        //return running;
    }
    
    return running;
}

void scheduler (int signum)
{
    assert (signum == SIGALRM);
    sys_time++;

    PCB* tocont = choose_process();

    dprintt ("continuing", tocont->pid);
    if (kill (tocont->pid, SIGCONT) == -1)
    {
        perror ("kill");
        return;
    }
}

void process_done (int signum)
{
    assert (signum == SIGCHLD);
    
    WRITE("---- entering child_done\n");

    for (;;)
    {
        int status, cpid;
        
        int time_start = running->started;
    
        int run_time = sys_time - time_start;
    
        cpid = waitpid (-1, &status, WNOHANG);
        
        dprintt ("in process_done", cpid);
    
        cout << "interrupted: " << running->interrupts << endl;
        
        cout << "context switched: " << running->switches << endl;
        
        cout << "process time: " << run_time << endl;

        if (cpid < 0)
        {
            WRITE("cpid < 0\n");
            kill (0, SIGTERM);
        }
        else if (cpid == 0)
        {
            WRITE("cpid == 0\n");
            break;
        }
        else
        {
            dprint (WEXITSTATUS (status));
            
            char buf[10];
            
            assert (eye2eh (cpid, buf, 10, 10) != -1);
            
            WRITE("process exited:");
            
            WRITE(buf);
            
            WRITE("\n");
            
            child_count++;
            
            if (child_count == NUM_CHILDREN)
            {
                kill (0, SIGTERM);
            }
        }
    }
    
    running->state = TERMINATED;

    running = idle;
    
    WRITE("---- leaving child_done\n");

    /*
    int status, cpid;
    
    int time_start = running->started;
    
    int run_time = sys_time - time_start;

    cpid = waitpid (-1, &status, WNOHANG);

    dprintt ("in process_done", cpid);
    
    cout << "interrupted: " << running->interrupts << endl;
    
    cout << "context switched: " << running->switches << endl;
    
    cout << "process time: " << run_time << endl;
    
    running->state = TERMINATED;
    
    running = idle;
    

    if  (cpid == -1)
    {
        perror ("waitpid");
    }
    else if (cpid == 0)
    {
        if (errno == EINTR) { return; }
        perror ("no children");
    }
    else
    {
        dprint (WEXITSTATUS (status));
    }
    */
}

char pipe_responce [1024];
char responce [2];

const char *handle_pipe (char* buffer)
{
    string buf = buffer;
    
    istringstream iss(buf);
    
    vector<string> tokens;
    
    string token;
    
    while (getline(iss, token, '.')) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    if (tokens[0] == "Request process time") {
        //const char *message = to_string(sys_time).c_str();
        strcpy (pipe_responce, "The system time is ");
        
        eye2eh (sys_time, responce, 2, 10);
        
        strcat(pipe_responce, responce);
        
        //cout << pipe_responce << endl;
               
    }
    if (tokens[1] ==  "Request process list") {
        
        strcat(pipe_responce, " The process list is ");
        
        list<PCB *> :: iterator it;
        
        for(it = processes.begin(); it != processes.end(); it++) 
        {
            strcat(pipe_responce, (*it)->name);
            
            strcat(pipe_responce, " ");
        }  
    
    } else {
        
        perror("help i died");
        exit(-1);
    }
    
    return pipe_responce;
}

//*****************copied out of main.cc for hw5*********************************
void process_trap (int signum)
{
    assert (signum == SIGTRAP);
    WRITE("---- entering process_trap\n");

    /*
    ** poll all the pipes as we don't know which process sent the trap, nor
    ** if more than one has arrived.
    */
    char buf[1024];
    int num_read = read (running->pipes[P2K][READ_END], buf, 1023);
    //cout << running->pipes[P2K][READ_END] << endl;
    if (num_read > 0)
    {
        buf[num_read] = '\0';
        WRITE("kernel read: ");
        WRITE(buf);
        WRITE("\n");
        
        const char *message = handle_pipe(buf);
        
        /*
        // respond
        const char *message = "from the kernel to the process";
        */
        
        write (running->pipes[K2P][WRITE_END], message, strlen (message));

    }

    WRITE("---- leaving process_trap\n");
}
//*******************************************************************************

/*
** stop the running process and index into the ISV to call the ISR
*/
void ISR (int signum)
{
    if (kill (running->pid, SIGSTOP) == -1)
    {
        perror ("kill");
        return;
    }
    dprintt ("stopped", running->pid);

    ISV[signum](signum);
}

/*
** set up the "hardware"
*/
void boot (int pid)
{
    ISV[SIGALRM] = scheduler;       create_handler (SIGALRM, ISR);
    ISV[SIGCHLD] = process_done;    create_handler (SIGCHLD, ISR);
//*****************************Code added****************************************
    ISV[SIGTRAP] = process_trap;    create_handler (SIGTRAP, ISR);

    // start up clock interrupt
    int ret;
    if ((ret = fork ()) == 0)
    {
        // signal this process once a second for three times
        send_signals (SIGALRM, pid, 1, NUM_SECONDS);

        // once that's done, really kill everything...
        kill (0, SIGTERM);
    }

    if (ret < 0)
    {
        perror ("fork");
    }
}

void create_idle ()
{
    int idlepid;

    if ((idlepid = fork ()) == 0)
    {
        dprintt ("idle", getpid ());

        // the pause might be interrupted, so we need to
        // repeat it forever.
        for (;;)
        {
            dmess ("going to sleep");
            pause ();
            if (errno == EINTR)
            {
                dmess ("waking up");
                continue;
            }
            perror ("pause");
        }
    }
    idle = new (PCB);
    idle->state = RUNNING;
    idle->name = "IDLE";
    idle->pid = idlepid;
    idle->ppid = 0;
    idle->interrupts = 0;
    idle->switches = 0;
    idle->started = sys_time;
}

//copying from creat_idle above
void create_process (char* program)
{    
    PCB* proc = new (PCB);
    proc->state = NEW;
    proc->name = program;
    proc->interrupts = 0;
    proc->switches = 0;
    proc->started = sys_time;
    
    // create the pipes
    /*
        int r1 = pipe2(proc->pipes[P2K], O_NONBLOCK);
        int r2 = pipe2(proc->pipes[K2P], O_NONBLOCK);
        assert(r1 == 0 && r1 ==0);
    */
    
    // i is from process to kernel, K2P from kernel to process
    assert (pipe (proc->pipes[P2K]) == 0);
    assert (pipe (proc->pipes[K2P]) == 0);

    // make the read end of the kernel pipe non-blocking.
    assert (fcntl (proc->pipes[P2K][READ_END], F_SETFL,
    fcntl(proc->pipes[P2K][READ_END], F_GETFL) | O_NONBLOCK) == 0);
    
    new_list.push_front(proc);
    //cout << "hi" << endl;
}

int main (int argc, char **argv)
{
    
    int pid = getpid();
    dprintt ("main", pid);

    sys_time = 0;

    boot (pid);

    // coming back to after create process
    if(argc > 0) 
    {
        for (int i = 1; i < argc; i++)
        {
            //for (int j = 0; j < NUM_PIPES; j++)
            //{
                create_process(argv[i]);
            //}
            
        }
    }

    // create a process to soak up cycles
    create_idle ();
    running = idle;

    cout << running;

    // we keep this process around so that the children don't die and
    // to keep the IRQs in place.
    for (;;)
    {
        pause();
        if (errno == EINTR) { continue; }
        perror ("pause");
    }
}
