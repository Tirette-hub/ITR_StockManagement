#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>

#include <signal.h>

#ifndef SYS_gettid
#error "SYS_gettid unavailable on this system"
#endif

#define gettid() ((pid_t)syscall(SYS_gettid))

#define SEM_PRIVATE 0

#define SHM_RDWR 0

#define __START__ 0
#define EXPECTING 1
#define OPERATING 2
#define __FINAL__ 3

#define DAY_DURATION 16
#define NIGHT_DURATION 8
#define UT 2000 //1Unit of Time = 2000ms => 1sec = .5 UT
//one day is represented by 24 unit of time

#define true 1
#define false 0;
#define product_number 4
#define client_number 2
typedef bool int;

//RT signals
#define SIGRTF (SIGRTMIN + 0) //SIGnal Real Time "Full"		//from productor to stock manager
//#define SIGRTC (SIGRTMIN + 0) //SIGnal Real Time "Clear"	//from stock manager to productor
#define SIGRTR (SIGRTMIN + 0) //SIGnal Real Time "Ready"	//from client to stock manager

//data structures
typedef struct Product{
	int id,
	volume;
	char descr[256];
} Product;
typedef struct Productor{
	int product_id;
	double production_time;
} Productor;
typedef struct Client{
	int id,
	min_time,
	max_time,
	*request;
} Client;

//behavior descriptions
void* ProductorBehavior(void*);
void ManagerBehavior();
void* ClientBehavior(void*);

//handlers
void handleQuit(int signum);
void handleFullProductorStock(int, siginfo_t*, void*);
void handleOrder(int, siginfo_t*, void*);

//getters
int getDeltaMili();
bool isDayTime();
int roundStock(double, int);

//setters
void createDataSet();


//global variables
pid_t other_pid;
int quit = 0, count = 0;
sem_t* semaphore;
int product_size = sizeof(Product), productor_size = sizeof(Productor), client_size = sizeof(Client);

//global variables only used by stock manager process
int shmid_tab[product_number];
int order_queue[client_number];

volatile sig_atomic_t status;
sigset_t mask;

Product products[product_number];
Productor productors[product_number];
Client clients[client_number];
int *stocks[product_number];
int stocks_parameters[2*product_number]; //4 first int: max size of each stock, 4 last int: current filling level
clock_t begin;

int main(int argc, char* argv[]){
	signal(SIGINT, handleQuit);
	sigfillset(&mask);
	other_pid = getpid();

	createDataSet();

	begin = clock();
	pid_t fvalue = fork();

	if (fvalue != 0){
		//gestionnaire
	}else{
		fvalue = fork();
		if (fvalue != 0){
			//producteurs
			pthread_t threads_list[product_number];
			//srand(time(NULL));
			sem_init(semaphore, SEM_PRIVATE, 0);
			//create threads
			for (int i = 0; i < product_number; i++)
				pthread_create(&threads_list[i], NULL, ProductorBehavior, NULL);

			int* retval;
			for (int i = 0; i < product_number; i++){
				pthread_join(threads_list[i], (void*)&retval);
			}
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


//behaviors

void* ProductorBehavior(void* unused){
	int thread_retval = EXIT_SUCCESS;

	//local variables
	//Product product = products[self.product_id];
	int shmid, serial_number = 1;
	union sigval envelope;
	unsigned int *local_stock, *serial_id;

	//link this thread to a specific productor defined previously
	//create key to create a shared memory
	sem_wait(semaphore);
	key_t ipc_key = ftok("projet.c", count);
	Productor self = productors[count++];
	sem_post(semaphore);

	envelope.sival_int = self.product_id;

	//create local stock
	shmid = shmget(ipc_key, 2*sizeof(int), IPC_CREAT | 0600); //check how to create the unique id for product identification
	//link shm addresses to pointers
	local_stock = shmat(shmid, NULL, 0);
	serial_id = local_stock+1;

	//stock is filled by a product, defined by its id instead of storing a new Product structure (gain of memory)
	*local_stock = self.product_id;

	//behavior loop
	while(!quit){
		while(!isDayTime() || *serial_id != -1){
			//wait for day time or the manager to empty the local stock
		}

		//product (new serial number)
		*serial_id = serial_number++;

		//send a signal to parent, notifying him a product is ready
		sigqueue(other_pid, SIGRTF, envelope);

		sleep(self.production_time);
	}

	//free the shm allocated
	if ( shmctl (shmid, IPC_RMID, NULL) == -1)
		perror ("shmctl");

	shmdt(local_stock);

	pthread_exit(&thread_retval);
}

void ManagerBehavior(){
	srand(time(NULL));

	//creating stocks
	int max_stock_volume = 100; //total volume

	//what we want to perform modularly from this data set
	/*int apple_stock[34];	//34V
	int pear_stock[23];		//23V
	int wood_stock[7];		//21V
	int brick_stock[11];	//22V //total = 100V
	stocks[0] = apple_stock;
	stocks[1] = pear_stock;
	stocks[2] = wood_stock;
	stocks[3] = brick_stock;*/

	//some math stuff while creating optimized stocks to do as described just befores
	int devider = 0;
	int checker = 0; //debug variable
	for (int i = 0; i < product_number; i++){
		//get production for a day:
		double day_production = DAY_DURATION*products[productors[i].product_id].volume*UT/(1000.0*productors[i].production_time);
		devider = devider + day_production;
	}
	//partitionning
	for (int i = 0; i < product_number; i++){
		int product_volume = products[productors[i].product_id].volume;
		double rebasement = DAY_DURATION*product_volume*UT*max_stock_volume/(1000.0*devider*productors[i].production_time); //change "percentage base" of day production
		int stock_base = (int)(rebasement - fmod(rebasement, product_volume));
		int stock_size = stock_base + roundStock(rebasement, product_volume);
		checker = checker + stock_size;
		int stock[stock_size/product_volume];
		stocks_parameters[i] = stock_size;
		stocks_parameters[i+product_number] = 0;
		stocks[i] = stock;
	}

	//creating shm
	for (int i = 0; i < product_number; i++)
		shmid_tab[i] = shmget(ftok("projet.c", i), 2*sizeof(int), IPC_CREAT | 0600); //get the shm id

	//create message queues to communicate with clients

	//expect signals from Productors or Clients
	struct sigaction descriptor;
	descriptor.sa_flags=SA_SIGINFO;
	descriptor.sa_sigaction = handleProductor;
	sigaction(SIGRTF, &descriptor, NULL);
	descriptor.sa_sigaction = handleClient;
	sigaction(SIGRTR, &descriptor, NULL);

	//principle loop
	while(!quit){

	}

	//free shm
	for (int i = 0; i < product_number; i++){
		shmctl(shmid, IPC_RMID, NULL);
	}
}

void* ClientBehavior(void* unused){
	int thread_retval = EXIT_SUCCESS;



	pthread_exit(&thread_retval);
}


//handlers

void handleQuit(int signum){
	if (semaphore != NULL){
		if (quit == 0){
			sem_wait(semaphore);
			quit = 1;
			sem_post(semaphore);
		}
	}
	else
		quit = 1;
}


//getters

int getDeltaMili(){
	clock_t now = clock();
	int delta = (end - begin) * 1000 /CLOCKS_PER_SEC;
	return delta;
}

bool isDayTime(){
	int delta = getDeltaMili();
	double time_passed = (double)delta / UT; //in UT
	double time_of_day = time_passed / (DAY_DURATION+NIGHT_DURATION);

	if (time_of_day <= DAY_DURATION)
		return true;
	
	return false;
}

int roundStock(double base, int product_volume){
	double rest = fmod(base, product_volume);
	if (rest <= product_volume / 2.0)
		return 0;
	return product_volume;
}


//setters

void createDataSet(){
	//create products
	Product apple = {
		0,
		1,
		"An apple every day keeps doctors away"
	};
	Product pear = {
		1,
		1,
		"La bonne pear"
	};
	Product wood = {
		2,
		3,
		"Un tronc tout frais"
	};
	Product brick = {
		3,
		2,
		"Utilisé dans la construction de maisons"
	};
	products[0] = apple;
	products[1] = pear;
	products[2] = wood;
	products[3] = brick;

	//create productors
	Productor apple_trees = {
		0,
		1
	};
	Productor pear_trees = {
		1,
		1.5
	};
	Productor forest = {
		2,
		5
	};
	Productor quarry = {
		3,
		3
	};
	productors[0] = apple_trees;
	productors[1] = pear_trees;
	productors[2] = forest;
	productors[3] = quarry;

	//create clients
	Client market = {
		0,
		2,
		10,
		{2, apple, 2, pear, 3}
	};
	Client mason = {
		1,
		5,
		15,
		{2, wood, 1, brick, 5}
	};
	clients[0] = market;
	clients[1] = mason;
}