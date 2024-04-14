#ifndef ERROR_H
#define ERROR_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
    #define _ERROR() fprintf(stderr, "errno = \"%s\".\n", strerror(errno))
#else
    #define _ERROR()
#endif

#define ERROR(...)                                             \
    do {                                                       \
        fprintf(stderr, "ERROR(%s:%d): ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                          \
        _ERROR();                                              \
        fflush(stderr);                                        \
        exit(1);                                               \
    } while (0)

#endif  // ERROR_H