#ifndef UI_H
#define UI_H

#include <stdlib.h>
#include "state.h"

void ui_init(void);
void ui_cleanup(void);

// Renders to buffer
void render(State *state);
// Outputs the buffer
void output(size_t scroll, size_t height);
// Calls `ch` on line at position `y`
void invoke_action(State *state, size_t y, int ch);

size_t get_screen_height(void);
size_t get_lines_length(void);

#endif  // UI_H