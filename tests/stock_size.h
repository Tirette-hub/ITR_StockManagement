#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#ifndef __MACROS__
#define __MACROS__
#define DAY_DURATION 16
#define NIGHT_DURATION 8
#define UT 2000 //1Unit of Time = 2000ms => 1sec = .5 UT
//one day is represented by 24 unit of time

#define product_number 4
#endif

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

int roundStock(double, int);

//setters
void createDataSet();

//global variables
Product products[product_number];
Productor productors[product_number];

//returns the total stock size
int *createStocks(int max_stock_volume, int stock_sizes[]){
	createDataSet();

	//creating stocks

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
	//printf("\rdevider = %i\n", devider);
	//partitionning
	for (int i = 0; i < product_number; i++){
		//printf("\rDBUG[%i]:\n", i);
		int product_volume = products[productors[i].product_id].volume;
		//printf("\tproduct volume = %i,\n\tproduction speed = %f,\n", product_volume, productors[i].production_time);
		double rebasement = DAY_DURATION*product_volume*UT*max_stock_volume/(1000.0*devider*productors[i].production_time); //change "percentage base" of day production
		//printf("\trebasement = %f,\n", rebasement);
		int stock_base = (int)(rebasement - fmod(rebasement, product_volume));
		//printf("\tstock base = %i,\n", stock_base);
		int stock_size = stock_base + roundStock(rebasement, product_volume);
		//printf("\tstock size = %i\n\n", stock_size);
		checker = checker + stock_size;

		stock_sizes[i] = stock_size;
	}

	stock_sizes[product_number] = checker;

	//printf("\rCheck stock size: %d\n", checker);

	return stock_sizes;
}


//getters

int roundStock(double base, int product_volume){
	//printf("\trounding: ");
	double rest = fmod(base, product_volume);
	//printf("rest = %f & mid = %f => ",rest, product_volume / 2.0);
	if (rest <= product_volume/2.0){
		//printf("0,\n");
		return 0;
	}
	//printf("%i,\n", product_volume);
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
}