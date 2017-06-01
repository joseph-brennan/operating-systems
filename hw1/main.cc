// hi worked with, helped, and got help from everyone in the collab room
#include <string>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>


using namespace std;
int main(int argc, char *argv[]) {

	pid_t pid = fork();

	if(pid < 0) {
	
		perror("PID failed \n");

		exit(EXIT_FAILURE);
	}
	if(pid == 0) {
		printf("Child PID: %ld\n", (long)getpid());

		execl("./counter","counter", "5", (char*)NULL);
		
		perror("execl \n");

		exit(errno);		
		
	} 
	else {
		printf("Parent PID: %ld\n", (long)getpid());

		int status;

		int w = waitpid(pid, &status, 0);


		if(WIFEXITED(status)) {
			printf("Process %ld ", (long)getpid());

			printf("exited with status: %d\n", WEXITSTATUS(status));
			exit(EXIT_SUCCESS);
		}
		if(w == -1) {
			perror("Failer at wait \n");

			exit(errno);
		}

	}

}


