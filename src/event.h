#ifndef EVENT_H
#define EVENT_H

#include <ncurses.h>
#include "git/state.h"

void poll_init(void);
void poll_cleanup(void);

// Polls for either file change or key. Returns whether key was pressed
bool poll_events(State *state);
void poll_ignore_change(void);

#endif  // EVENT_H