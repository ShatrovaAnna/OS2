#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void* newPrinter(void *args){
    while (1) {
        printf("I'm a thread! ");
    }
}

void printErr(int err){
	char buf[100];
	strerror_r(err, buf, 100);
	printf("Error: %s", buf);
  exit(-1);
}

int main() {
	
	pthread_t thread;
	
	int err = pthread_create(&thread, NULL, newPrinter, NULL);
	if (err) {
		printErr(err);
	}

    sleep(2);

    err = pthread_cancel(thread);
	  if (err){
        printErr(err);
    }
	
    printf("\n");

   
    return 0;
}
