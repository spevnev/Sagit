#ifndef ERROR_H
#define ERROR_H

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef DEBUG
    #define ERROR2() fprintf(stderr, "ERROR(%s:%d): ", __FILE__, __LINE__);
#else
    #define ERROR2() fprintf(stderr, "ERROR: ");
#endif

#define ERROR(...)                    \
    do {                              \
        endwin();                     \
        ERROR2();                     \
        fprintf(stderr, __VA_ARGS__); \
        fflush(stderr);               \
        _exit(1);                     \
    } while (0)

#ifdef DEBUG
    #define ASSERT(condition)                                                                    \
        do {                                                                                     \
            if (!(condition)) {                                                                  \
                endwin();                                                                        \
                fprintf(stderr, "ASSERT(%s) failed at %s:%d\n", #condition, __FILE__, __LINE__); \
                fflush(stderr);                                                                  \
                _exit(1);                                                                        \
            }                                                                                    \
        } while (0)
#else
    #define ASSERT(condition)
#endif

#define UNREACHABLE()                                                              \
    do {                                                                           \
        endwin();                                                                  \
        fprintf(stderr, "UNREACHABLE was reached at %s:%d\n", __FILE__, __LINE__); \
        fflush(stderr);                                                            \
        _exit(1);                                                                  \
    } while (0)

#define OUT_OF_MEMORY() ERROR("Process is out of memory.\n")

#endif  // ERROR_H