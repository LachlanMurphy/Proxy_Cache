// derived from https://github.com/LachlanMurphy/MultiLookupPA6.git

#ifndef DARRAY_H
#define DARRAY_H

#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <malloc.h>

#define BUFFSIZE 1024

typedef struct {
    char file_name[BUFFSIZE];
    time_t last_used;
} delement;

typedef struct {
    delement* arr;
    int size;
    int max_size;
    sem_t mutex;
    int timeout;
} darray;

// initialize the array
int  darray_init(darray *s, int timeout);

// place element into the array, block when full
int  darray_put (darray *s, char *name);

// free the array's resources
void darray_free(darray *s);

// checks all elemnts to see if they have expired
void darray_clean(darray *s);

void darray_poke(darray *s, char* name);

#endif // DARRAY_H