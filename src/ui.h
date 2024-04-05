#ifndef UI_H
#define UI_H

#include <stdlib.h>
#include "state.h"

#define MOUSE_UP 1 << 19
#define MOUSE_DOWN 1 << 27

void ui_init(void);
void ui_cleanup(void);

// Renders to buffer
void render(State *state);
// Outputs the buffer
void output(size_t scroll);
// Calls action associated with `ch` on element at position `y`
int invoke_action(size_t y, int ch);
void output_help(size_t scroll);

size_t get_screen_height(void);
size_t get_lines_length(void);
size_t get_help_length(void);

#endif  // UI_H