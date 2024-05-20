// Simple dynamic array implementation using macros.
// NOTE: You can't take pointers to vector elements due to realloc calls.

#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>
#include "error.h"

#define INITIAL_VECTOR_CAPACITY 16

#define VECTOR_TYPEDEF(name, type) \
    typedef struct {               \
        size_t capacity;           \
        size_t length;             \
        type *data;                \
    } name

// "Removes" all elements without changing capacity and changing memory
#define VECTOR_RESET(vec) (vec)->length = 0;

#define VECTOR_FREE(vec)     \
    do {                     \
        free((vec)->data);   \
        (vec)->length = 0;   \
        (vec)->capacity = 0; \
        (vec)->data = NULL;  \
    } while (0);

#define VECTOR_PUSH(vec, element)                                                       \
    do {                                                                                \
        ASSERT((vec) != NULL);                                                          \
        if ((vec)->capacity == 0) {                                                     \
            (vec)->capacity = INITIAL_VECTOR_CAPACITY;                                  \
            (vec)->data = malloc((vec)->capacity * sizeof(*(vec)->data));               \
            if ((vec)->data == NULL) OUT_OF_MEMORY();                                   \
        } else if ((vec)->length == (vec)->capacity) {                                  \
            (vec)->capacity *= 2;                                                       \
            (vec)->data = realloc((vec)->data, (vec)->capacity * sizeof(*(vec)->data)); \
            if ((vec)->data == NULL) OUT_OF_MEMORY();                                   \
        }                                                                               \
        (vec)->data[(vec)->length++] = (element);                                       \
    } while (0)

VECTOR_TYPEDEF(str_vec, char *);
VECTOR_TYPEDEF(size_vec, size_t);
VECTOR_TYPEDEF(int_vec, int);

#endif  // VECTOR_H
