#include "dynamic_array.h"

int darray_init(darray *s, int timeout) {
	if (s == NULL) return -1;

	sem_init(&s->mutex, 0, 1);
	s->size = 0;
	s->max_size = 1;
	s->arr = (delement *) malloc(1 * sizeof(delement));
	s->timeout = timeout;

	return 0;
}

int  darray_put (darray *s, char *name) {
	sem_wait(&s->mutex);
		for (int i = 0; i < s->size; i++) {
			if (s->arr[i].last_used == 0) {
				// add element
				s->arr[i].last_used = time(NULL);
				strncpy(s->arr[i].file_name, name, BUFFSIZE);
				sem_post(&s->mutex);
				return 0;
			}
		}
		if (s->size >= s->max_size) {
			// reallocate memmory
			s->arr = realloc(s->arr, s->max_size*2*sizeof(delement));
			s->max_size *= 2;
		}
		s->arr[s->size].last_used = time(NULL);
		strncpy(s->arr[s->size].file_name, name, BUFFSIZE);
		s->size++;
	sem_post(&s->mutex);
	return 0;
}

// free the array's resources
void darray_free(darray *s) {
	free(s->arr);
	sem_destroy(&s->mutex);
}

// checks all elemnts to see if they have expired
void darray_clean(darray *s) {
	sem_wait(&s->mutex);
		for (int i = 0; i < s->size; i++) {
			if (s->arr[i].last_used != 0 && time(NULL) - s->arr[i].last_used > s->timeout) {
				// remove unit
				s->arr[i].last_used = 0;
				FILE* file = fopen(s->arr[i].file_name, "r");
				if (file) remove(s->arr[i].file_name);
			}
		}
	sem_post(&s->mutex);
}

void darray_poke(darray *s, char* name) {
	sem_wait(&s->mutex);
		for (int i = 0; i < s->size; i++) {
			if (!strncmp(name, s->arr[i].file_name, strlen(name))) {
				s->arr[i].last_used = time(NULL);
				break;
			}
		}
	sem_post(&s->mutex);
}