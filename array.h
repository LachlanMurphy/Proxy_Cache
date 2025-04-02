// derived from https://github.com/LachlanMurphy/MultiLookupPA6.git

#ifndef ARRAY_H
#define ARRAY_H

#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#define ARRAY_SIZE 1

typedef struct {
    pthread_t * arr[ARRAY_SIZE];
    int size;
    sem_t mutex;
    sem_t available_items;
    sem_t free_items;
} array;

// initialize the array
int  array_init(array *s);

// place element into the array, block when full
int  array_put (array *s, pthread_t *thread);

// remove element from the array, block when empty
int  array_get (array *s, pthread_t *thread_id);

// free the array's resources
void array_free(array *s);

// put dummy sigkill at bottom of stack
void array_end(array *s, char *signal);

// print contents of array for debugging
void print_array(array *s);

#endif // ARRAY_H