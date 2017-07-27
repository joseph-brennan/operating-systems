#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>

#define READ 0
#define WRITE 1

#define TO_KERNEL 3
#define FROM_KERNEL 4

using namespace std;

int main (int argc, char** argv)
{
    int pid = getpid();
    
    int ppid = getppid();

    printf ("writing in pid %d\n", pid);
    
    char pipe_out[1024];
    
    strcpy(pipe_out, "Request process time.");
    
    strcat(pipe_out, "Request process list.");
    
    //const char *message = "Request process time";
    
    //const char *message1 = "Request process list";
    
    const char *message = pipe_out;
    
    write (TO_KERNEL, message, strlen (message));

    printf ("trapping to %d in pid %d\n", ppid, pid);
    
    //cout<<"!!! " << ppid << endl;
    kill (ppid, SIGTRAP);
    

    printf ("reading in pid %d\n", pid);
    
    char buf[1024];
    
    //char buf1[1024];
    
    int num_read = read (FROM_KERNEL, buf, 1023);
    
    //int sec_read = read (FROM_KERNEL, buf1, 1023);
    
    buf[num_read] = '\0';
    
    //buf[sec_read] = '\0';
    
    printf ("process %d read: %s\n", pid, buf);
    
    //printf ("process %d read: %s\n", pid, buf1);
    

    exit (0);
}
