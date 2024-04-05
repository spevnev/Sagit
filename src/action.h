#ifndef ACTION_H
#define ACTION_H

#include "state.h"

// clang-format off
static const int AC_RERENDER      = 1 << 0;
static const int AC_UPDATE_STATE = 1 << 1;
// clang-format on

// enum SectionType { SC_UNTRACKED, SC_UNSTAGED, SC_STAGED };

int section_action(void *section, int ch);
int file_action(void *file, int ch);
int hunk_action(void *hunk, int ch);
int line_action(void *line, int ch);

#endif  // ACTION_H