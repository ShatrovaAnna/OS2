#include <stdio.h>
#include <pthread.h>
#include <synch.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

pthread_cond_t cond;
pthread_mutex_t mutex;
bool activeChild = false;
bool activeParent = true;

void printErr(int err){
	char buf[100];
	strerror_r(err, buf, 100);
	printf("Error: %s", buf);
    	exit(-1);
}

void* newPrint(void* arg){
	pthread_mutex_lock(&mutex);

	for(int i = 0; i < 5; i++){
		
		printf("I'm a CHILD!\n");
		activeChild = !activeChild;
		
		int err = pthread_cond_signal(&cond);
		if(err){
			printErr(err);
		}
		while(!activeParent){ 
			err = pthread_cond_wait(&cond, &mutex); 
			if(err){
				printErr(err);
			}
		}
		activeParent = !activeParent;
	}
	return NULL;
}

int main(){
	
	pthread_t thread;

	int err = pthread_mutex_init(&mutex, NULL);
	if(err){
		printErr(err);
	}
	err = pthread_cond_init(&cond, NULL);
	if(err){
		pthread_mutex_destroy(&mutex);
		printErr(err);
	}
	pthread_mutex_lock(&mutex);
	
	err = pthread_create(&thread, NULL, newPrint, NULL);
    	if (err) {
		pthread_mutex_destroy(&mutex);
		pthread_cond_destroy(&cond);
        	printErr(err);
    	}

	for(int i = 0; i < 5; i++){
		
		printf("I'm a PARENT!\n");
		activeParent = !activeParent;
		if(i != 0) {
			err = pthread_cond_signal(&cond);
			if(err){
				pthread_mutex_destroy(&mutex);
				pthread_cond_destroy(&cond);
				printErr(err);
			}
		}
		while(!activeChild){ 
			err = pthread_cond_wait(&cond, &mutex); 
			if(err){
				pthread_mutex_destroy(&mutex);
				pthread_cond_destroy(&cond);
				printErr(err);
			}
		}
		activeChild = !activeChild;
	}
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
	return 0;
}
