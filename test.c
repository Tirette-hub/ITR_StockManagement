#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "tests/stock_size.h"

#ifndef __MACROS__
#define __MACROS__
#define DAY_DURATION 16
#define NIGHT_DURATION 8
#define UT 2000 //1Unit of Time = 2000ms => 1sec = .5 UT
//one day is represented by 24 unit of time

#define product_number 4
#endif

typedef enum{false = 0, true = 1} bool;

int main(int argc, char* argv[]){

	int count = 0;

	bool isOk = true;

	if (isOk){
		int expected_tab[5] = {34, 23, 7, 11, 100};
		int result_tab[5];
		createStocks(100, result_tab);
		for (int i = 0; i < 5; i++){
			if (result_tab[i] != expected_tab[i])
				isOk = false;
		}

		if (isOk)
			count++;
	}


	if (!isOk){
		switch(count){
			case 0:
			printf("failed testing createStocks function.\n");
			break;
		}
		return EXIT_FAILURE;
	}else{
		printf("all tests performed successfully!\n");
		return EXIT_SUCCESS;
	}
}