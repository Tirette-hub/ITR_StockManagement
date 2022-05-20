# ARCHITECTURE
![project architecture](/schema_archi_projet.jpg)
_The **SIGRT** arrow from **SM** to **P** is not in use anymore_  

# HOW TO RUN
```
gcc projet.c -lm -pthread -o projet && ./projet
```
*do not try now, not tested yet, just read the code*

# WHAT DOES IT CONTAINS
## DATA STRUCTURES
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

## PRODUCTS
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

## PRODUCTORS
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

## CLIENTS
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