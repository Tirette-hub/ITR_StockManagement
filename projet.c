#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>

#include <signal.h>

#define SEM_SHARED

#define SHM_RDWR 0

#define __START__ 0
#define EXPECTING 1
#define OPERATING 2
#define __FINAL__ 3

//RT signals

//data structures
struct Product{
	int id,
	volume;
	char descr[256];
};
struct Productor{
	int product_id;
	double production_time;
};
struct Client{
	int id,
	min_time,
	max_time,
	*request;
};

//behavior descriptions
void* ProductorBehavior(void*);
void ManagerBehavior();
void* ClientBehavior(void*);

//handlers
void handleQuit(int signum);


//global variables
pid_t other_pid;
int shmid;
volatile sig_atomic_t status = __START__;
unsigned int* segment;
sigset_t mask;

int product_number = 4;
struct Product products[product_number];
struct Productor productors[product_number]
struct Client clients[2];

int main(int argc, char* argv[]){
	sigfillset(&mask);
	other_pid = getpid();

	//create products

	pid_t fvalue = fork();

	if (fvalue != 0){
		//gestionnaire
	}else{
		signal(SIGINT, handleQuit);
		fvalue = fork();
		if (fvalue != fvalue){
			//producteurs
		}else{
			//clients
		}
	}
	return EXIT_SUCCESS;
}

void* ProductorBehavior(void* unused){

}

void ManagerBehavior(){

}

void* ClientBehavior(void* unused){

}

void handleQuit(int signum){
	if ( shmctl (shmid, IPC_RMID, NULL) == -1)
		perror ("shmctl");

	shmdt(segment);
	exit(EXIT_SUCCESS);
}