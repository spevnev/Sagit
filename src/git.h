#ifndef GIT_H
#define GIT_H

#include "state.h"

int is_git_initialized(void);

void get_git_state(State *state);
int is_state_empty(State *state);
void update_git_state(State *state);

void git_stage_file(char *file);
void git_unstage_file(char *file);

void git_stage_hunk(const File *file, const Hunk *hunk);
void git_unstage_hunk(File *file, Hunk *hunk);

#endif  // GIT_H