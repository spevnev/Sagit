#ifndef ACTION_H
#define ACTION_H

#include "state.h"

typedef struct {
    int ch;
    int range_start;
    int range_end;
} ActionArgs;

typedef int action_t(void *, const ActionArgs *);

typedef struct {
    const File *file;
    Hunk *hunk;
} HunkArgs;

typedef struct {
    const File *file;
    Hunk *hunk;
    int hunk_y;
    int line;
} LineArgs;

// clang-format off
#define AC_RERENDER         (1 << 0)
#define AC_UPDATE_STATE     (1 << 1)
#define AC_TOGGLE_SELECTION (1 << 2)
// clang-format on

int section_action(void *section, const ActionArgs *args);

int untracked_file_action(void *file_path, const ActionArgs *args);
int unstaged_file_action(void *file, const ActionArgs *args);
int staged_file_action(void *file, const ActionArgs *args);

int unstaged_hunk_action(void *hunk_args, const ActionArgs *args);
int staged_hunk_action(void *hunk_args, const ActionArgs *args);

int unstaged_line_action(void *line_args, const ActionArgs *args);
int staged_line_action(void *line_args, const ActionArgs *args);

#endif  // ACTION_H