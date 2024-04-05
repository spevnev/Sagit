#ifndef GIT_H
#define GIT_H

#include "state.h"

int is_git_initialized(void);
void get_git_state(State *state);
int is_state_empty(State *state);

#endif  // GIT_H