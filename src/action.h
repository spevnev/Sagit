#ifndef ACTION_H
#define ACTION_H

#include "state.h"

// clang-format off
static const int AC_RERENDER      = 1 << 0;
static const int AC_UPDATE_STATE = 1 << 1;
// clang-format on

int section_action(void *section, int ch);

int untracked_file_action(void *file_path, int ch);
int unstaged_file_action(void *file, int ch);
int staged_file_action(void *file, int ch);

int unstaged_hunk_action(void *hunk, int ch);
int staged_hunk_action(void *hunk, int ch);

int unstaged_line_action(void *line, int ch);
int staged_line_action(void *line, int ch);

#endif  // ACTION_H