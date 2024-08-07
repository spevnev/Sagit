#ifndef STATE_H
#define STATE_H

#include <ncurses.h>
#include "ctxt.h"
#include "vector.h"

typedef struct {
    bool is_folded;
    const char *header;
    str_vec lines;
} Hunk;

VECTOR_TYPEDEF(HunkVec, Hunk);

typedef enum { FC_CREATED = 1, FC_DELETED, FC_MODIFIED, FC_RENAMED } FileChange;

typedef struct {
    bool is_folded;
    bool is_binary;
    FileChange change_type;
    const char *src;
    const char *dst;
    const char *old_mode;
    const char *new_mode;
    HunkVec hunks;
} File;

VECTOR_TYPEDEF(FileVec, File);

typedef struct {
    bool is_folded;
    char *raw;
    FileVec files;
} Section;

typedef struct {
    MemoryContext untracked_ctxt;
    Section unstaged;
    Section staged;
} State;

void free_files(FileVec *files);
void free_state(State *state);

#endif  // STATE_H
