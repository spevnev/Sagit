#ifndef ERROR_H
#define ERROR_H

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ERROR(...)                                             \
    do {                                                       \
        endwin();                                              \
        fprintf(stderr, "ERROR(%s:%d): ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                          \
        fflush(stderr);                                        \
        _exit(1);                                              \
    } while (0)

#define ASSERT(condition)                                                                    \
    do {                                                                                     \
        if (!(condition)) {                                                                  \
            endwin();                                                                        \
            fprintf(stderr, "ASSERT(%s) failed at %s:%d\n", #condition, __FILE__, __LINE__); \
            fflush(stderr);                                                                  \
            _exit(1);                                                                        \
        }                                                                                    \
    } while (0)

#define UNREACHABLE()                                                              \
    do {                                                                           \
        endwin();                                                                  \
        fprintf(stderr, "UNREACHABLE was reached at %s:%d\n", __FILE__, __LINE__); \
        fflush(stderr);                                                            \
        _exit(1);                                                                  \
    } while (0)

#endif  // ERROR_H