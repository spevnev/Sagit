#ifndef STATE_H
#define STATE_H

#include "utils/vector.h"

typedef struct {
    int is_folded;
    const char *header;
    str_vec lines;
} Hunk;

DEFINE_VECTOR_TYPE(HunkVec, Hunk);

enum FileChangeType { FC_CREATED = 1, FC_DELETED, FC_MODIFIED, FC_RENAMED };

typedef struct {
    int is_folded;
    char change_type;
    const char *src;
    const char *dest;
    HunkVec hunks;
} File;

DEFINE_VECTOR_TYPE(FileVec, File);

// NOTE: Actual sections (from State) must have the same layout
typedef struct {
    int is_folded;
} Section;

typedef struct {
    struct {
        int is_folded;

        char *raw;
        str_vec files;
    } untracked;
    struct {
        int is_folded;

        char *raw;
        FileVec files;
    } unstaged;
    struct {
        int is_folded;

        char *raw;
        FileVec files;
    } staged;
} State;

void free_files(FileVec *files);
void free_state(State *state);

#endif  // STATE_H