# Introduction
This academic project has been writen in the context of the "Informatique en Temps Réel" lecture or _Real-Time Computing_ at the University of Mons.
It aims accomodating with memory, processus, threads and inter-process communication management.\
The goal is to construct 3 process:\
- 1. Manager Processus: It manages all the orders that can be yield by clients via message queues. It also create a shared memory to represent stocks of productors in which the manager will pick the goods to fill orders of clients.
- 2. Productor Processus: It can product some goods, identified by an id, a volume, and has a description. Itself is identified by the product id it produces and a production time. It also has a limited stock size which must be emptied prior the productor can produce again.\
- 3. Client Processus: It sends static orders at random intervals of time to the manager. Each order is built with the product id the client wants and the quantity. The list has then _de facto_ an even length.
_A day and night system is modelised as well. During night, Manager does not work and productions stop._

# Architecture
The Manager's processus creates the shared memory that represents the stocks and wait the Client's processus to open the message queues. The stocks have optimized size based on the productors' production time.\
The Productor's processus waits the Manager's pocessus for the shared memory to be created prior to begin the _production_. As soon as a good has been produced, the productor asks for a semaphore on the shared memory and put the product in it so that the manager can access it.\
The Client's processus waits for the manager to create the message queues. When done it starts waiting in a random dynamic interval of time and sends static orders to the manager via the message queues.

# How to run
```
gcc projet.c -lm -pthread -lrt -o projet && sudo ./projet
```
*CTRL-C to stop the execution or it will run undefinitly*

# How to test
```
gcc test.c -lm -o test && ./test
```
Tests creation of stocks and their size for *Stock Manager* process

# What does a stock contains
## Data structures
### 1. PRODUCT
id (*int*)  
volume (*int*)  
descr (*char\[256\]*)
### 2. PRODUCTOR
product_id (*int*)  
production_time (*double*)  
### 3. CLIENT
id (*int*)  
min_time (*int*)  
max_time (*int*)  
request (*int\**)  

## Products
### 1. POMME
*id:* 0  
*volume:* 1  
*descr:* An apple every day keeps doctors away  
### 2. POIRE
*id:* 1  
*volume:* 1  
*descr:* La bonne poire  
### 3. BOIS
*id:* 2  
*volume:* 3  
*descr:* Un tronc tout frais  
### 4. BRIQUE
*id:* 3  
*volume:* 2  
*descr:* Utilisé dans la construction de maisons  

## Productors
### 1. POMMIERS
*product_id:* 0  
*production_time:* 1  
### 2. POIRIERS
*product_id:* 1  
*production_time:* 1.5  
### 3. FORET
*product_id:* 2  
*production_time:* 5  
### 4. CARRIERE
*product_id:* 3  
*production_time:* 3  

## Clients
### 1. MARAICHER
*id:* 0  
*min_time:* 2  
*max_time:* 10  
*request:* {**POMME**, 2, **POIRE**, 3}  
### 2. MACON
*id:* 1  
*min_time:* 5  
*max_time:* 15  
*request:* {**BOIS**, 1, **BRIQUE**, 5}  
