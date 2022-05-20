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

#define SEM_PRIVATE 0

#define SHM_RDWR 0

#define __START__ 0
#define EXPECTING 1
#define OPERATING 2
#define __FINAL__ 3

#define DAY_DURATION 16
#define NIGHT_DURATION 8
//one day is represented by 24 unit of time

//RT signals

//data structures
typedef struct Product{
	int id,
	volume;
	char descr[256];
};
typedef struct Productor{
	int product_id;
	double production_time;
};
typedef struct Client{
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

//getters
int getDeltaMili();


//global variables
pid_t other_pid;
int shmid;
volatile sig_atomic_t status = __START__;
unsigned int* segment;
sigset_t mask;
sem_t* semaphore;
int product_number = 4, client_number = 2, count = 0;
Product products[product_number];
Productor productors[product_number];
Client clients[client_number];
clock_t begin;

int main(int argc, char* argv[]){
	sigfillset(&mask);
	other_pid = getpid();

	//create products
	Product pomme = {
		0;
		1;
		"An apple every day keeps doctors away";
	}
	Product poire = {
		1;
		1;
		"La bonne poire";
	}
	Product bois = {
		2;
		3;
		"Un tronc tout frais";
	}
	Product brique = {
		3;
		2;
		"Utilisé dans la construction de maisons";
	}
	products[0] = pomme;
	products[1] = poire;
	products[2] = bois;
	products[3] = brique;

	//create productors
	Productor pommiers = {
		0;
		1;
	}
	Productor poiriers = {
		1;
		1.5;
	}
	Productor foret = {
		2;
		5;
	}
	Productor carriere = {
		3;
		3;
	}
	productors[0] = pommiers;
	productors[1] = poiriers;
	productors[2] = foret;
	productors[3] = carriere;

	//create clients
	Client maraicher = {
		0;
		2;
		10;
		{2, pomme, 2, poire, 3};
	}
	Client macon = {
		1;
		5;
		15;
		{2, bois, 1, brique, 5};
	}

	begin = clock();
	pid_t fvalue = fork();

	if (fvalue != 0){
		//gestionnaire
	}else{
		signal(SIGINT, handleQuit);
		fvalue = fork();
		if (fvalue != fvalue){
			//producteurs
			srand(time(NULL));
			sem_init(semaphore, SEM_PRIVATE, 0);
			//create threads

			sem_destroy(semaphore);
		}else{
			srand(time(NULL));
			//clients
			sem_init(semaphore, SEM_PRIVATE, product_number + 10);
			//create threads

			sem_destroy(semaphore);
		}
	}
	return EXIT_SUCCESS;
}

void* ProductorBehavior(void* unused){
	sem_wait(semaphore);
	struct Productor self = {
		count++;
		; //every second
	}
	sem_post(semaphore);
}

void ManagerBehavior(){
	srand(time(NULL));
}

void* ClientBehavior(void* unused){

}

void handleQuit(int signum){
	if ( shmctl (shmid, IPC_RMID, NULL) == -1)
		perror ("shmctl");

	shmdt(segment);
	exit(EXIT_SUCCESS);
}

int getDeltaMili(){
	clock_t now = clock();
	int delta = (end - begin) * 1000 /CLOCKS_PER_SEC;
	return delta;
}