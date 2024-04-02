#ifndef GIT_H
#define GIT_H

#include "vector.h"

typedef struct {
    char is_folded;

    char *header;
    str_vec lines;
} Hunk;

DEFINE_VECTOR_TYPE(HunkVec, Hunk);

typedef struct {
    char is_folded;

    char *src;
    char *dest;
    HunkVec hunks;
} File;

DEFINE_VECTOR_TYPE(FileVec, File);

// NOTE: Actual sections (from State) must have the same layout
typedef struct {
    char is_folded;
} Section;

typedef struct {
    struct {
        char is_folded;

        char *raw;
        str_vec files;
    } untracked;
    struct {
        char is_folded;

        char *raw;
        FileVec files;
    } unstaged;
    struct {
        char is_folded;

        char *raw;
        FileVec files;
    } staged;
} State;

int is_git_initialized(void);
int get_git_state(State *state);
void free_files(FileVec *vec);

#endif  // GIT_H