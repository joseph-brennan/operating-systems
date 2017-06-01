#include <string>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>

using namespace std;
int main(int argc, char *argv[]) {

	
	long number = strtol(argv[1], NULL, 10);
	
	for(int i = 0; i < number; i++) {
		long pid = getpid();
	
		cout << "Process: " << pid << " " << i + 1 << '\n';
		
	}
	
	return number;

}
