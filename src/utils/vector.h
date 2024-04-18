// Simple dynamic array implementation using macros.
// NOTE: You can't take pointers to vector elements due to realloc calls.

#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

#define INITIAL_VECTOR_CAPACITY 16

#define DEFINE_VECTOR_TYPE(name, type) \
    typedef struct {                   \
        size_t capacity;               \
        size_t length;                 \
        type *data;                    \
    } name

// "Removes" all elements without changing capacity and changing memory
#define VECTOR_RESET(vec) (vec)->length = 0;

#define VECTOR_FREE(vec)     \
    do {                     \
        free((vec)->data);   \
        (vec)->length = 0;   \
        (vec)->capacity = 0; \
    } while (0);

#define VECTOR_PUSH(vec, element)                                                    \
    do {                                                                             \
        if ((vec)->capacity == 0) {                                                  \
            (vec)->capacity = INITIAL_VECTOR_CAPACITY;                               \
            (vec)->data = malloc((vec)->capacity * sizeof((element)));               \
            if ((vec)->data == NULL) ERROR("Process is out of memory.\n");           \
        } else if ((vec)->length == (vec)->capacity) {                               \
            (vec)->capacity *= 2;                                                    \
            (vec)->data = realloc((vec)->data, (vec)->capacity * sizeof((element))); \
            if ((vec)->data == NULL) ERROR("Process is out of memory.\n");           \
        }                                                                            \
        (vec)->data[(vec)->length++] = (element);                                    \
    } while (0)

#define VECTOR_POP(vec)            \
    do {                           \
        ASSERT((vec)->length > 0); \
        (vec)->length--;           \
    } while (0)

DEFINE_VECTOR_TYPE(str_vec, char *);
DEFINE_VECTOR_TYPE(void_vec, void *);

#endif  // VECTOR_H