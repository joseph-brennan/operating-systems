#include <signal.h>
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <sys/wait.h>
#include <errno.h>
using namespace std;

void sig_handler(int);


int main(int argc, char *argv[])
{
	struct sigaction *action = new (struct sigaction);
	sigemptyset (&(action->sa_mask));
	action->sa_handler = sig_handler;
	
	if(sigaction(SIGHUP, action, NULL) < 0) {
		perror("Error with sigaction");

		delete action;

		exit(EXIT_FAILURE);
	}

	if(sigaction(SIGABRT, action, NULL) < 0) {
		perror("Error with sigaction");

		delete action;

		exit(EXIT_FAILURE);
	}

	if(sigaction(SIGSEGV, action, NULL) < 0) {
		perror("Error with sigaction");

		delete action;

		exit(EXIT_FAILURE);
	}


	int PPid = getpid();

	pid_t CPid = fork();

	if (CPid == -1) {

		//error
	}
	else if(CPid == 0) { 
		//chiild
		kill(PPid, SIGHUP);
		kill(PPid, SIGABRT);
		kill(PPid, SIGSEGV);
		kill(PPid, SIGSEGV);
		kill(PPid, SIGSEGV);
/*
		kill(SIGHUP, PPid);
		kill(SIGABRT, PPid);
		kill(SIGSEGV, PPid);
		kill(SIGSEGV, PPid);
		kill(SIGSEGV, PPid);
*/



	} else { 
		//parent

		int status;
		do {
			int w = waitpid(CPid, &status, 0);
		} while (!WIFEXITED(status));
	}

}

void sig_handler(int sig) {

	switch(sig) {
		case 1:
		signal(SIGHUP, sig_handler);

		cout << "sighup case called" << endl;

		break;

		case 6:
		signal(SIGABRT, sig_handler);

		cout << "sigabrt case called" << endl;
		break;

		case 11:
		signal(SIGSEGV, sig_handler);

		cout << "sigsegv case called" << endl;
		break;
	}
}
