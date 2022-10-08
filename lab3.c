#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

void *newPrinter(void *args) {

    char **temp;
    for(temp=(char **)args; *temp!=NULL; temp++) {
        printf("%s\n", *temp);
    }
    return NULL;
}

void printErr(int err){
	char buf[100];
	strerror_r(err, buf, 100);
	printf("Error: %s", buf);
    exit(-1);
}

int main() {
	
   pthread_t threads[4];

   int err;

   static char *param_1[]={ "Thread 1 line 1", "Thread 1 line 2", "Thread 1 line 3", NULL };
   static char *param_2[]={ "Thread 2 line 1", "Thread 2 line 2", "Thread 2 line 3", NULL };
   static char *param_3[]={ "Thread 3 line 1", "Thread 3 line 2", "thread 3 line 3", NULL };
   static char *param_4[]={ "Thread 4 line 1", "Thread 4 line 2", "Thread 4 line 3", NULL };

	err = pthread_create(&threads[0], NULL, newPrinter, param_1);
    if (err) {
        printErr(err);
    }
    err = pthread_create(&threads[1], NULL, newPrinter, param_2);
    if (err) {
        printErr(err);
    }
    err = pthread_create(&threads[2], NULL, newPrinter, param_3);
    if (err) {
        printErr(err);
    }
    err = pthread_create(&threads[3], NULL, newPrinter, param_4);
    if (err) {
        printErr(err);
    }

    for (int i = 0; i < 4; i++){
        err = pthread_join(threads[i], NULL);
        if (err){
			printErr(err);
        }
    }

    pthread_exit(0);
}
