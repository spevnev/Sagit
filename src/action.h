#ifndef ACTION_H
#define ACTION_H

#include "state.h"

typedef struct {
    int ch;
    int range_start;
    int range_end;
} ActionArgs;

typedef int action_t(void *, ActionArgs *);

// clang-format off
static const int AC_RERENDER         = 1 << 0;
static const int AC_UPDATE_STATE     = 1 << 1;
static const int AC_TOGGLE_SELECTION = 1 << 2;
// clang-format on

int section_action(void *section, ActionArgs *args);

int untracked_file_action(void *file_path, ActionArgs *args);
int unstaged_file_action(void *file, ActionArgs *args);
int staged_file_action(void *file, ActionArgs *args);

typedef struct {
    File *file;
    Hunk *hunk;
} HunkArgs;

int unstaged_hunk_action(void *hunk_args, ActionArgs *args);
int staged_hunk_action(void *hunk_args, ActionArgs *args);

typedef struct {
    File *file;
    Hunk *hunk;
    int hunk_y;
    int line;
} LineArgs;

int unstaged_line_action(void *line_args, ActionArgs *args);
int staged_line_action(void *line_args, ActionArgs *args);

#endif  // ACTION_H