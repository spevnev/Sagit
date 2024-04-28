#ifndef GIT_H
#define GIT_H

#include <ncurses.h>
#include "git/state.h"

static const char *NO_NEWLINE = "\\ No newline at end of file";

bool is_git_initialized(void);
bool is_state_empty(State *state);
bool is_ignored(char *file_path);

void get_git_state(State *state);
void update_git_state(State *state);

void git_stage_file(const char *file);
void git_unstage_file(const char *file);

void git_stage_hunk(const File *file, const Hunk *hunk);
void git_unstage_hunk(const File *file, const Hunk *hunk);

void git_stage_range(const File *file, const Hunk *hunk, int range_start, int range_end);
void git_unstage_range(const File *file, const Hunk *hunk, int range_start, int range_end);

#endif  // GIT_H