#include <signal.h>
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <sys/wait.h>
#include <errno.h>
#include <assert.h>
using namespace std;

void sig_handler(int);


int main(int argc, char *argv[])
{
	struct sigaction *action = new (struct sigaction);
	sigemptyset (&(action->sa_mask));

	action->sa_handler = sig_handler;

	assert (sigaction (SIGHUP, action, NULL) == 0);

	assert (sigaction (SIGABRT, action, NULL) == 0);

	assert (sigaction (SIGSEGV, action, NULL) == 0);

/**	if(sigaction(SIGHUP, action, NULL) < 0) {
		perror("Error with sighup");

		delete action;

		exit(EXIT_FAILURE);
	}

	if(sigaction(SIGABRT, action, NULL) < 0) {
		perror("Error with sigabrt");

		delete action;

		exit(EXIT_FAILURE);
	}

	if(sigaction(SIGSEGV, action, NULL) < 0) {
		perror("Error with sigsegv");

		delete action;

		exit(EXIT_FAILURE);
	}
*/



	pid_t CPid = fork();

	if (CPid == -1) {
		perror("fork()");

		delete action;

		exit(EXIT_FAILURE);
		//error
	}
	else if(CPid == 0) { 
		//chiild
		int PPid = getppid();

		kill(PPid, SIGHUP);
		//cout << "hi 1" << endl;
		kill(PPid, SIGABRT);
		//cout << "hi 2" << endl;
		kill(PPid, SIGSEGV);
		//cout << "hi 3" << endl;
		kill(PPid, SIGSEGV);
		//cout << "hi 4" << endl;
		kill(PPid, SIGSEGV);
		//cout << "hi 5" << endl;

	} else { 
		//parent
/*		while(1) {
			int status;

			int retpid = waitpid(CPid, &status, 0);

			if (retpid == -1) {
				if (errno == EINTR) {
					continue;
				}

				perror("waitpid failed");

				break;
			}

			assert(retpid == CPid);
		}

*/
		int status;

		do {


			int w = waitpid(CPid, &status, 0);

			if(w == -1) {
				if(errno == EINTR) {
					continue;
				}
				perror("waitpid failed");

				//cout << "waitpid() Failed" << errno << endl;

				break;
			}

			//assert(w == CPid);


		} while (!WIFEXITED(status));

		delete action;
		exit(EXIT_SUCCESS);
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
