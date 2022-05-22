#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>
#include <mqueue.h>

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

#define product_number 4
#define client_number 2
typedef enum{false, true} bool;

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
char* format(char*, int);

//setters
void createDataSet();


//global variables
pid_t other_pid;
int quit = 0, count = 0;
sem_t* semaphore;
int product_size = sizeof(Product), productor_size = sizeof(Productor), client_size = sizeof(Client);

Product products[product_number];
Productor productors[product_number]; //as many productors as products
Client clients[client_number];
clock_t begin;

//global variables only used by stock manager process
int shmid_tab[product_number];
int *segment_tab[2*product_number]; //4first int: product id, 4 last int: serial number (for every productor)
int *stocks[product_number];
int stocks_parameters[2*product_number]; //4 first int: max size of each stock, 4 last int: current filling level (for every stock of each type)
int order_queue[client_number];

int main(int argc, char* argv[]){

	signal(SIGINT, handleQuit);
	other_pid = getpid();

	createDataSet();

	begin = clock();
	pid_t fvalue = fork();

	if (fvalue != 0){
		srand(time(NULL));
		//gestionnaire
		ManagerBehavior();
	}else{
		fvalue = fork();
		if (fvalue != 0){
			//producteurs
			pthread_t threads_list[product_number];
			sem_init(semaphore, SEM_PRIVATE, 0);
			//create threads
			for (int i = 0; i < product_number; i++)
				pthread_create(&threads_list[i], NULL, ProductorBehavior, NULL);

			int* retval;
			for (int i = 0; i < product_number; i++)
				pthread_join(threads_list[i], (void*)&retval);

			sem_destroy(semaphore);
		}else{
			srand(time(NULL) ^ (getpid() << 16));
			//clients
			pthread_t threads_list[client_number];
			sem_init(semaphore, SEM_PRIVATE, product_number + 10);
			//create threads
			for (int i = 0; i < client_number; i++)
				pthread_create(&threads_list[i], NULL, ClientBehavior, NULL);
			
			int *retval;
			for (int i = 0; i < client_number; i++)
				pthread_join(threads_list[i], (void*)&retval);

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
	shmdt(serial_id);

	pthread_exit(&thread_retval);
}

void ManagerBehavior(){
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
	//int checker = 0; //debug variable
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
		int stock_size = stock_base + roundStock(rebasement, product_volume); //true stock size
		//checker = checker + stock_size;
		stock_size = stock_size/product_volume; //stock size relative to its product volume //allows to restrict the size to the number of products it can contain
		int stock[stock_size/product_volume];
		stocks_parameters[i] = stock_size;
		stocks_parameters[i+product_number] = 0;
		stocks[i] = stock;
	}

	//creating shm
	for (int i = 0; i < product_number; i++){
		shmid_tab[i] = shmget(ftok("projet.c", i), 2*sizeof(int), IPC_CREAT | 0600); //get the shm id
		segment_tab[i] = shmat(shmid_tab[i], NULL, 0);
		segment_tab[i+product_number] = segment_tab[i]+1;
	}

	//create message queues to communicate with clients
	mqd_t message_queues[client_number];

	char* s;
	for (int i = 0; i < client_number; i++){
		do{
			sprintf(s, "/c%i-queue", i);
			message_queues[i] = mq_open(s, O_WRONLY);
		}while(message_queues[i] == -1);
	}

	//expect signals from Productors or Clients
	struct sigaction descriptor;
	descriptor.sa_flags=SA_SIGINFO;
	descriptor.sa_sigaction = handleProductor;
	sigaction(SIGRTF, &descriptor, NULL);
	descriptor.sa_sigaction = handleClient;
	sigaction(SIGRTR, &descriptor, NULL);

	//principle loop
	while(!quit){
		if (isDayTime()){
			for (int i = 0; i < count; i++){ //count is used here to count the number of orders
				//check if the order can be honored
				int j = 0;
				int client_id = order_queue[i];
				int product_list[] = clients[client_id].request;
				int number_of_products = product_list[j++];
				bool deliverable = true;
				for (int i = 0; i < number_of_products && deliverable; i++)
					if (stocks_parameters[product_number+product_list[1+i].id] < product_list[i+2])
						deliverable = false;

				//empty stocks
				for (int i = 0; i < number_of_products; i++){
					int number_to_send = number_of_products[i+2];
					for (int j = 0; j < number_to_send; j++)
						stocks_parameters[product_number+product_list[1+i].id]--;
				}
				//by changing only the id pointing to the "empty" stock case, filling again this case will overwrite the previous serial number

				if (deliverable){
					int status = mq_send(message_queues[client_id], s, strlen(s), 0);
					if (status == -1)
						printf("Error trying to send message via queue: %s to client n_%i\n", s, client_id);
				}
			}
		}
	}

	//free shm
	for (int i = 0; i < product_number; i++){
		shmctl(shmid_tab[i], IPC_RMID, NULL);
		shmdt(segment_tab[i]);
		shmdt(segment_tab[i+product_number]);
	}

	//close message queues
	for(int i = 0; i < client_number; i++)
		mq_close(message_queues[i]);
}

void* ClientBehavior(void* unused){
	int thread_retval = EXIT_SUCCESS;

	sem_wait(semaphore);
	Client self = clients[count++];
	sem_psot(semaphore);

	int min_time = self.min_time;
	int max_time = self.max_time;

	int id = self.id;

	int priority = 0;
	const char buffer[1024];

	union sigval envelope;
	envelope.sival_int = self.id;

	char* mq_name;

	//Open message queue
	sprintf(mq_name, "/c%i-queue", id);
	mqd_t queue = mq_open(mq_name, O_CREAT | O_RDONLY);
	if(queue == -1){
		perror("mq_open");
		return EXIT_FAILURE;
	}

	while(!quit){
		//Wait before send request
		int wait_time = (rand() % (max_time - min_time)) + min_time;
		sleep(wait_time);


		//Send signal to stock with the request
		sigqueue(other_pid, SIGRTR, envelope);

		size_t amount;

		// Wait for queue to be filled
		do{
			amount = mq_receive(queue, buffer, 1024, &priority);
		}while(amount == -1);

		printf("[%d] received order: %s");

	}

	mq_close(queue);
	mq_unlink(mq_name);

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

void handleFullProductorStock(int signum, siginfo_t* info, void* context){
	int productor_id = info->si_value.sival_int;

	//int *product_id = segment_tab[productor_id];
	int *serial_number = segment_tab[productor_id+product_number];

	if (stocks_parameters[productor_id + product_number] < stocks_parameters[productor_id]){
		stocks[productor_id][stocks_parameters[productor_id + product_number]] = *serial_number;
		stocks_parameters[productor_id + product_number]++;
		*serial_number = -1;
	}
}

void handleOrder(int signum, siginfo_t* info, void* context){
	int client_id = info->si_value.sival_int;
	order_queue[count++] = client_id;
}


//getters

int getDeltaMili(){
	clock_t now = clock();
	int delta = (now - begin) * 1000 /CLOCKS_PER_SEC;
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

char* format(char* s, int value){
	char* retval;
	if (s == NULL){
		sprintf(retval, "%i", value);
	}else{
		sprintf(retval, "%s %i", s, value);
	}

	return retval;
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
	int order[5] = {2, apple.id, 2, pear.id, 3};
	Client market = {
		0,
		2,
		10,
		order
	};
	order[0] = 2;
	order[1] = wood.id;
	order[2] = 1;
	order[3] = brick.id;
	order[4] = 5;
	Client mason = {
		1,
		5,
		15,
		order
	};
	clients[0] = market;
	clients[1] = mason;
}