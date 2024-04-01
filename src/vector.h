#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

#define INITIAL_VECTOR_CAPACITY 16

#define DEFINE_VECTOR_TYPE(name, type) \
    typedef struct {                   \
        size_t capacity;               \
        size_t length;                 \
        type *data;                    \
    } name;

#define VECTOR_PUSH(vec, element)                                                  \
    do {                                                                           \
        if ((vec)->capacity == 0) {                                                \
            (vec)->capacity = INITIAL_VECTOR_CAPACITY;                             \
            (vec)->data = malloc((vec)->capacity * sizeof(element));               \
            if ((vec)->data == NULL) ERROR("Process is out of memory.");           \
        } else if ((vec)->length == (vec)->capacity) {                             \
            (vec)->capacity *= 2;                                                  \
            (vec)->data = realloc((vec)->data, (vec)->capacity * sizeof(element)); \
            if ((vec)->data == NULL) ERROR("Process is out of memory.");           \
        }                                                                          \
        (vec)->data[(vec)->length++] = element;                                    \
    } while (0);

#endif  // VECTOR_H