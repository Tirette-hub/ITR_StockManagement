#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>
#include <mqueue.h>
#include <errno.h>

#include <signal.h>

/*#ifndef SYS_gettid
#error SYS_gettid unavailable on this system
#endif

#define gettid() ((pid_t)syscall(SYS_gettid))*/

#define SEM_PRIVATE 0
#define SEM_SHARED 1

#define SHM_RDWR 0

#define __START__ 0
#define EXPECTING 1
#define EXPECTING_P 1
#define EXPECTING_C 2
#define OPERATING 3
#define __FINAL__ 4

#define DAY_DURATION 16
#define NIGHT_DURATION 8
#define UT 2 //1Unit of Time = 2sec => 1sec = .5 UT
//one day is represented by 24 unit of time

#define product_number 4
#define client_number 2
typedef enum{false = 0, true = 1} bool;

//RT signals
#define SIGRTR (SIGRTMIN + 0)	//SIGnal Real Time "Ready"		//from client to stock manager
#define SIGRTF (SIGRTMIN + 1)	//SIGnal Real Time "Full"		//from productor to stock manager
#define SIGRTC (SIGRTMIN + 2)	//SIGnal Real Time "Client"		//from client to stock manager
#define SIGRTP (SIGRTMIN + 3)	//SIGnal Real Time "Productor"	//from productor to stock manager

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
void handleQuit(int, siginfo_t*, void*);
void handleFullProductorStock(int, siginfo_t*, void*);
void handleOrder(int, siginfo_t*, void*);
void handleClientStarted(int, siginfo_t*, void*);
void handleProductorStarted(int, siginfo_t*, void*);
void handleReady(int, siginfo_t*, void*);

//getters
int getDeltaMili();
int isDayTime();
int roundStock(double, int);
void format(char*, int);
void printStocks();

//setters
void createDataSet();


//global variables
volatile sig_atomic_t status = __START__;
sigset_t mask;

union sigval envelope;

pid_t other_pid, productors_pid, clients_pid;
int quit = 0, count = 0;
sem_t semaphore;
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
	sigfillset(&mask);
	//signal(SIGINT, handleQuit);
	struct sigaction descriptor;
	descriptor.sa_flags=SA_SIGINFO;
	descriptor.sa_sigaction = handleQuit;
	sigaction(SIGALRM, &descriptor, NULL);
	struct itimerval cfg;
	cfg.it_value.tv_sec = 30;
	other_pid = getpid();

	createDataSet();

	/*for (int i = 0; i < client_number; i++){
		printf("[Client %i] request size: %i\n", i, clients[i].request[0]);
		for (int j = 0; j < clients[i].request[0]; j++){
			printf("\r%i: %i\n", clients[i].request[1+2*j], clients[i].request[1+2*j+1]);
		}
	}*/

	begin = clock();
	pid_t fvalue = fork();

	if (fvalue != 0){
		//srand(time(NULL));
		//gestionnaire
		sigdelset(&mask, SIGALRM);
		sigprocmask(SIG_SETMASK, &mask, NULL);
		setitimer(ITIMER_REAL, &cfg, NULL);
		printf("\r[%ld]Creating stock manager\n", getpid());
		ManagerBehavior();
	}else{
		fvalue = fork();
		struct sigaction descriptor;
		descriptor.sa_flags=SA_SIGINFO;
		descriptor.sa_sigaction = handleReady;
		sigaction(SIGRTR, &descriptor, NULL);

		sigdelset(&mask, SIGRTR);
		sigprocmask(SIG_SETMASK, &mask, NULL);

		if (fvalue != 0){
			//producteurs
			sigdelset(&mask, SIGALRM);
			sigprocmask(SIG_SETMASK, &mask, NULL);
			setitimer(ITIMER_REAL, &cfg, NULL);
			printf("\r[%ld]creating productors\n", getpid());
			pthread_t threads_list[product_number];
			sem_init(&semaphore, SEM_PRIVATE, 1);

			//create threads
			for (int i = 0; i < product_number; i++)
				pthread_create(&threads_list[i], NULL, ProductorBehavior, NULL);

			int* retval;
			for (int i = 0; i < product_number; i++)
				pthread_join(threads_list[i], (void*)&retval);

			sem_destroy(&semaphore);
		}else{
			srand(time(NULL) ^ (getpid() << 16));
			//clients
			sigdelset(&mask, SIGALRM);
			sigprocmask(SIG_SETMASK, &mask, NULL);
			setitimer(ITIMER_REAL, &cfg, NULL);
			printf("\r[%ld]Creating clients\n", getpid());
			pthread_t threads_list[client_number];
			sem_init(&semaphore, SEM_PRIVATE, product_number + 1);

			//create threads
			for (int i = 0; i < client_number; i++)
				pthread_create(&threads_list[i], NULL, ClientBehavior, NULL);
			
			int *retval;
			for (int i = 0; i < client_number; i++)
				pthread_join(threads_list[i], (void*)&retval);

			sem_destroy(&semaphore);
		}
	}

	for (int i = 0; i < client_number; i++)
		free(clients[i].request);

	printf("\rfinishing process\n");
	return EXIT_SUCCESS;
}


//behaviors

void* ProductorBehavior(void* unused){
	int thread_retval = EXIT_SUCCESS;

	status = EXPECTING;

	//local variables
	//Product product = products[self.product_id];
	int shmid, serial_number = 1;
	unsigned int *local_stock, *serial_id;

	//link this thread to a specific productor defined previously
	//create key to create a shared memory
	sem_wait(&semaphore);
	//printf("\r[Productor %i] created\n", count);
	key_t ipc_key = ftok("projet.c", count);
	Productor self = productors[count++];
	sem_post(&semaphore);

	//create local stock
	shmid = shmget(ipc_key, 2*sizeof(int), IPC_CREAT | 0666); //check how to create the unique id for product identification
	//link shm addresses to pointers
	local_stock = shmat(shmid, NULL, 0);
	serial_id = local_stock+1;

	//stock is filled by a product, defined by its id instead of storing a new Product structure (gain of memory)
	*local_stock = self.product_id;

	*serial_id = -1;

	sem_wait(&semaphore);
	if (self.product_id == product_number-1){
		envelope.sival_int = self.product_id;
		sigqueue(other_pid, SIGRTP, envelope);
	}
	sem_post(&semaphore);

	while(status == EXPECTING){
		//wait stock manager to tell he is ok
	}

	//behavior loop
	while(status == OPERATING){
		while(!isDayTime() || *serial_id != -1){
			//wait for day time or the manager to empty the local stock
			if (status == __FINAL__){
				//free the shm allocated
				if ( shmctl (shmid, IPC_RMID, NULL) == -1)
					perror ("shmctl");

				shmdt(local_stock);
				shmdt(serial_id);

				pthread_exit(&thread_retval);
			}
		}

		//product (new serial number)
		*serial_id = serial_number++;

		printf("\r[Productor %i] producting (%i)\n", self.product_id, *serial_id);

		//send a signal to parent, notifying him a product is ready
		//printf("\r[Productor %i] notifying the stock manager the production\n", self.product_id);
		sem_wait(&semaphore);
		envelope.sival_int = self.product_id;
		sigqueue(other_pid, SIGRTF, envelope);
		sem_post(&semaphore);

		sleep(self.production_time);
	}

	printf("\r[Productor %i] production stopped\n", self.product_id);

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

	struct sigaction descriptor;
	descriptor.sa_flags=SA_SIGINFO;
	descriptor.sa_sigaction = handleFullProductorStock;
	sigaction(SIGRTF, &descriptor, NULL);
	descriptor.sa_sigaction = handleOrder;
	sigaction(SIGRTR, &descriptor, NULL);
	descriptor.sa_sigaction = handleClientStarted;
	sigaction(SIGRTC, &descriptor, NULL);
	descriptor.sa_sigaction = handleProductorStarted;
	sigaction(SIGRTP, &descriptor, NULL);

	status = EXPECTING_P;

	//expect productors and clients to tell Stock Manager they have started
	sigdelset(&mask, SIGRTP);
	sigdelset(&mask, SIGRTC);
	sigdelset(&mask, SIGRTF);
	sigdelset(&mask, SIGRTR);
	sigprocmask(SIG_SETMASK, &mask, NULL);
	//sigprocmask(SIG_SETMASK, &mask, NULL);

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
		double day_production = DAY_DURATION*products[productors[i].product_id].volume*UT/(productors[i].production_time);
		devider = devider + day_production;
	}
	//partitionning
	for (int i = 0; i < product_number; i++){
		int product_volume = products[productors[i].product_id].volume;
		double rebasement = DAY_DURATION*product_volume*UT*max_stock_volume/(devider*productors[i].production_time); //change "percentage base" of day production
		int stock_base = (int)(rebasement - fmod(rebasement, product_volume));
		int stock_size = stock_base + roundStock(rebasement, product_volume); //true stock size
		//checker = checker + stock_size;
		stock_size = stock_size/product_volume; //stock size relative to its product volume //allows to restrict the size to the number of products it can contain
		//printf("\r[Stock Manager] stock %i, size: %i\n", i, stock_size);
		stocks_parameters[i] = stock_size;
		stocks_parameters[i+product_number] = 0;
		stocks[i] = malloc(stock_size*sizeof(int));

		for(int j = 0; j < stock_size; j++){
			stocks[i][j] = -1;
		}
	}
	//printf("\r[Stock Manager] stock has been partitionned\n");

	//printf("\r[Stock Manager] expecting signal from productor for ready status\n");
	while(status == EXPECTING_P){
		//wait for productors to be ready
		if (status == __FINAL__){
			return;
		}
	}
	//creating shm
	for (int i = 0; i < product_number; i++){
		shmid_tab[i] = shmget(ftok("projet.c", i), 2*sizeof(int), IPC_CREAT | 0666); //get the shm id
		segment_tab[i] = shmat(shmid_tab[i], NULL, 0);
		segment_tab[i+product_number] = segment_tab[i]+1;
	}
	//send to productors ready stocks status
	sigqueue(productors_pid, SIGRTR, envelope);

	//create message queues to communicate with clients
	mqd_t message_queues[client_number];

	//expects clients to send ready status
	/*sigdelset(&mask, SIGRTC);
	sigprocmask(SIG_SETMASK, &mask, NULL);*/

	while(status == EXPECTING_C){
		//wait for clients to be ready
		if (status == __FINAL__){
			//free shm
			for (int i = 0; i < product_number; i++){
				shmctl(shmid_tab[i], IPC_RMID, NULL);
				shmdt(segment_tab[i]);
				shmdt(segment_tab[i+product_number]);
			}
			return;
		}
	}

	char *s = malloc(1024*sizeof(char));
	for (int i = 0; i < client_number; i++){
		//printf("\r[Stock Manager] Opening client %i message queue\n", i);
		do{
			sprintf(s, "/c%i-queue", i);
			message_queues[i] = mq_open(s, O_WRONLY);
		}while(message_queues[i] == -1);
	}
	//send clients ready message queues status
	sigqueue(clients_pid, SIGRTR, envelope);

	bzero(s, 1024);

	//expect signals from Productors or Clients

	//principle loop
	while(status == OPERATING){
		//if (isDayTime()){
			int n = 0;
			for (int i = 0; i < count; i++){ //count is used here to count the number of orders
				//check if the order can be honored
				int client_id = order_queue[i];
				//printf("\r[Stock Manager] pending order from %i\n", client_id);
				Client client = clients[client_id];
				int number_of_products = client.request[0];
				bool deliverable = true;
				for (int i = 0; i < number_of_products; i++){
					if (stocks_parameters[product_number+client.request[1+2*i]] < client.request[2*i+2]){
						//printf("\r\tnot enough products in stocks\n");
						deliverable = false;
						break;
					}
				}

				if (deliverable){
					printf("\r[Stock Manager] found a deliverable order\n");
					//printStocks();
					//empty stocks
					for (int i = 0; i < number_of_products; i++){
						int number_to_send = client.request[2*i+2];
						printf("\r[Stock Manager] number of product %i to send: %i\n", client.request[2*i+1], number_to_send);
						for (int j = 0; j < number_to_send; j++){
							format(s, stocks_parameters[product_number+client.request[1+2*i]]);
							sprintf(s, "%s\0", s);
							stocks_parameters[product_number+client.request[1+2*i]]--;
							stocks[client.request[2*i+1]][stocks_parameters[product_number+client.request[1+2*i]]] = -1;
						}
					}
					//printStocks();
					//by changing only the id pointing to the "empty" stock case, filling again this case will overwrite the previous serial number

					//printf("\r[Stock Manager] test: %i\n", strlen(s));
					int status = mq_send(message_queues[client_id], s, strlen(s), 0);
					printf("\r[Stock Manager] order (%s) has been sent to client\n", s);
					if (status == -1){
						/*if (errno == EINTR){
							printf("\r[Stock Manager] error sys interruption\n");
							status = __FINAL__;
							break;
						}
						else{
							perror("mq_send");
						}*/
						perror("mq_send");
					}

					n++;

					bzero(s, 1024);
				}
			}
			
			if (n == 2)
				count = 0;
			else if (n == 1){
				if (count == 2)
					order_queue[0] = order_queue[1];

				count--;
			}
		//}
	}

	printStocks();

	sigaddset(&mask, SIGRTR);
	sigaddset(&mask, SIGRTF);

	printf("\r[Stock Manager] free shm and message queues\n");

	//free shm
	for (int i = 0; i < product_number; i++){
		shmctl(shmid_tab[i], IPC_RMID, NULL);
		shmdt(segment_tab[i]);
		shmdt(segment_tab[i+product_number]);
		free(stocks[i]);
	}

	free(s);

	//close message queues
	for(int i = 0; i < client_number; i++)
		mq_close(message_queues[i]);
}

void* ClientBehavior(void* unused){
	int thread_retval = EXIT_SUCCESS;

	status = EXPECTING;

	sem_wait(&semaphore);
	//printf("\r[Client %i] created\n", count);
	Client self = clients[count++];
	sem_post(&semaphore);

	int min_time = self.min_time;
	int max_time = self.max_time;

	int id = self.id;

	int priority = 0;
	char buffer[1024];

	char* mq_name = malloc(10*sizeof(char));

	//Open message queue
	sprintf(mq_name, "/c%i-queue", self.id);
	printf("\r[Client %i] creating message queue \"%s\"\n", self.id, mq_name);
	mqd_t queue = mq_open(mq_name, O_CREAT | O_RDONLY | O_NONBLOCK);
	struct mq_attr mqa;

	if(queue == -1){
		printf("\rmq_open error\n");
		thread_retval = EXIT_FAILURE;
		quit = __FINAL__;
		sem_wait(&semaphore);
		sigaddset(&mask, SIGRTR);
		sigprocmask(SIG_SETMASK, &mask, NULL);
		sem_post(&semaphore);
	}

	if(self.id == client_number-1){
		sem_wait(&semaphore);
		envelope.sival_int = self.id;
		sigqueue(other_pid, SIGRTC, envelope); //could maybe be removed
		sem_post(&semaphore);
	}

	while(status == EXPECTING){
		//wait for stock manager to send ready status
	}

	//printf("\r[Client %i] main loop\n", self.id);

	while(status == OPERATING){
		//Wait before send request
		int wait_time = (rand() % (max_time - min_time)) + min_time;
		printf("\r[Client %i] wait for %isec before to send an order\n", self.id, wait_time);
		sleep(wait_time);


		//printf("\r[Client %i] sending signal to stock manager (same thing as sending an order)\n", self.id);
		//Send signal to stock with the request
		sem_wait(&semaphore);
		envelope.sival_int = self.id;
		sigqueue(other_pid, SIGRTR, envelope);
		sem_post(&semaphore);

		size_t amount;

		// Wait for queue to be filled
		printf("\r[Client %i] waiting for stock manager to send back the order\n", self.id);
		do{
			mq_getattr(queue, &mqa);
			amount = mq_receive(queue, buffer, mqa.mq_msgsize, &priority);
			if (amount == -1){
				printf("\r[Client %i] ", self.id);
				perror("mq_receive");
			}
		}while(amount == -1 && status == OPERATING);

		printf("\r[Client %i] received order: %s\n", self.id, buffer);

	}

	printf("\r[Client %i] closing message queues\n", self.id);
	mq_close(queue);
	mq_unlink(mq_name);

	free(mq_name);

	printf("\r[Client %i] done\n", self.id);

	pthread_exit(&thread_retval);
}


//handlers

void handleQuit(int signum, siginfo_t* info, void* context){
	sigaddset(&mask, SIGALRM);
	sigprocmask(SIG_SETMASK, &mask, NULL);
	printf("\rquitting\n");
	if (status != __FINAL__){
		status = __FINAL__;
	}
}

void handleFullProductorStock(int signum, siginfo_t* info, void* context){
	sigaddset(&mask, SIGRTF);
	sigprocmask(SIG_SETMASK, &mask, NULL);

	int productor_id = info->si_value.sival_int;

	printf("\r[Stock Manager] received a full stock signal from productor %i\n", productor_id);

	//int *product_id = segment_tab[productor_id];
	int *serial_number = segment_tab[productor_id+product_number];

	//printf("\r[Stock Manager] will try to stock the new product (%i) if possible\n", *serial_number);
	if (stocks_parameters[productor_id + product_number] < stocks_parameters[productor_id]){
		//printf("\r[Stock Manager] pick up new product %i\n", productor_id);
		stocks[productor_id][stocks_parameters[productor_id + product_number]] = *serial_number;
		//printf("\r[Stock Manager] adding this product to the stocks [%i][%i]\n", productor_id, stocks_parameters[productor_id + product_number]);
		stocks_parameters[productor_id + product_number]++;
		//printf("\r[Stock Manager] notifying the productor %i his local stock is now empty and go to next place in stock: %i\n", productor_id, stocks_parameters[productor_id + product_number]);
		*serial_number = -1;
	}
	else{
		printf("\r[Stock Manager] no more place\n");
		printStocks();
	}
}

void handleOrder(int signum, siginfo_t* info, void* context){
	int client_id = info->si_value.sival_int;
	order_queue[count++] = client_id;

	printf("\r[Stock Manager] received an order signal from client %i\n", client_id);
}

void handleReady(int signum, siginfo_t* info, void* context){
	//procmask
	sigaddset(&mask, SIGRTR);
	sigprocmask(SIG_SETMASK, &mask, NULL);

	printf("\r[%ld] stock manager is ready\n", getpid());

	sem_wait(&semaphore);
	if (status == EXPECTING)
		status = OPERATING;
	sem_post(&semaphore);
}

void handleClientStarted(int signum, siginfo_t* info, void* context){
	//procmask
	clients_pid = info->si_pid;

	printf("\r[Stock Manager] client is ready: %i\n", info->si_pid);

	sigaddset(&mask, SIGRTC);
	sigprocmask(SIG_SETMASK, &mask, NULL);

	while(status != EXPECTING_C){
	}
	status = OPERATING;
}

void handleProductorStarted(int signum, siginfo_t* info, void* context){
	//procmask
	productors_pid = info->si_pid;

	printf("\r[Stock Manager] productor is ready: %i\n", info->si_pid);

	sigaddset(&mask, SIGRTP);
	sigprocmask(SIG_SETMASK, &mask, NULL);

	status = EXPECTING_C;
}


//getters

int getDeltaMili(){
	clock_t now = clock();
	int delta = (now - begin) * 1000 /CLOCKS_PER_SEC;
	return delta;
}

int isDayTime(){
	int delta = getDeltaMili();
	double time_passed = (double)delta / UT; //in UT
	double time_of_day = fmod(time_passed, (DAY_DURATION+NIGHT_DURATION));

	if (time_of_day <= DAY_DURATION)
		return 1;
	
	return 0;
}

int roundStock(double base, int product_volume){
	double rest = fmod(base, product_volume);
	if (rest <= product_volume / 2.0)
		return 0;
	return product_volume;
}

void format(char* s, int value){
	//char* retval;
	if (s == NULL){
		sprintf(s, "%i", value);
	}else if(strcmp(s, "") == 0){
		sprintf(s, "%i", value);
	}
	else{
		sprintf(s, "%s %i", s, value);
	}

	//return retval;
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
	int order1[5] = {2, apple.id, 2, pear.id, 3};
	Client market = {
		0,
		2,
		10,
		malloc(5*sizeof(int))
	};
	for (int i = 0; i < 2*order1[0]+1; i++){

		market.request[i] = order1[i];
	}
	int order2[5] = {2, wood.id, 1, brick.id, 5};
	Client mason = {
		1,
		5,
		15,
		malloc(5*sizeof(int))
	};
	for (int i = 0; i < 2*order2[0]+1; i++){
		mason.request[i] = order2[i];
	}
	clients[0] = market;
	clients[1] = mason;

	/*for (int i = 0; i < client_number; i++){
		printf("[Client %i] request size: %i\n", i, clients[i].request[0]);
		for (int j = 0; j < clients[i].request[0]; j++){
			printf("\r%i: %i\n", clients[i].request[1+2*j], clients[i].request[1+2*j+1]);
		}
	}*/
}

void printStocks(){
	for (int i = 0; i < product_number; i++){
		printf("Stock[%i]:{", i);
		for (int j = 0; j < stocks_parameters[i]; j++){
			printf("%i,", stocks[i][j]);
		}
		putchar('}');
		printf("\n");
	}
}