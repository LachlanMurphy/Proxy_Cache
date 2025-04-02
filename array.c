#include "array.h"


int  array_init(array *s) {
    if (s == NULL) return -1;

    sem_init(&s->mutex, 0, 1);
    sem_init(&s->available_items, 0, ARRAY_SIZE);
    sem_init(&s->free_items, 0, 0);
    s->size = 0;
    return 0;
}

int  array_put (array *s, pthread_t *thread) {
    sem_wait(&s->available_items);
      sem_wait(&s->mutex);
            // s->arr[s->size++] = thread;
            for (int i = 0; i < ARRAY_SIZE; i++) {
              if (s->arr[i] == NULL) {
                s->arr[i] = thread;
                s->size++;
                break;
              }
            }
      sem_post(&s->mutex);
    sem_post(&s->free_items);
    return 0;
}

int  array_get (array *s, pthread_t *thread_id) {
    sem_wait(&s->free_items);
      sem_wait(&s->mutex);
        for (int i = 0; i < ARRAY_SIZE; i++) {
          if (s->arr[i] == thread_id) {
            s->arr[i] = NULL;
            s->size--;
            break;
          }
        }
      sem_post(&s->mutex);
    sem_post(&s->available_items);
    return 0;
}

void array_free(array *s) {
    sem_destroy(&s->available_items);
    sem_destroy(&s->free_items);
    sem_destroy(&s->mutex);
}

void array_end(array *s, char *signal) {
    sem_wait(&s->available_items);
      sem_wait(&s->mutex);
            // strncpy(s->arr[s->size++], s->arr[0], MAX_NAME_LENGTH);
            // strncpy(s->arr[0], signal, MAX_NAME_LENGTH);
      sem_post(&s->mutex);
    sem_post(&s->free_items);
}

void print_array(array *s) {
    for (int i = 0; i < ARRAY_SIZE; i++) {
      printf("[%d]: %p\n", i, (void *) s->arr[i]);
    }
}