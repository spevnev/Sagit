#ifndef GIT_H
#define GIT_H

#include "vector.h"

typedef struct {
    char *src;
    char *dest;
    char **hunks;
} File;

DEFINE_VECTOR_TYPE(FileVec, File);

typedef struct {
    struct {
        char *raw;
        str_vec files;
    } untracked;
    struct {
        char *raw;
        FileVec files;
    } unstaged;
    struct {
        char *raw;
        FileVec files;
    } staged;
} GitState;

int is_git_initialized(void);
int get_git_state(GitState *git);

#endif  // GIT_H