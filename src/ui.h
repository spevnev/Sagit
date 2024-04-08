#ifndef UI_H
#define UI_H

#include <stdlib.h>
#include "state.h"

#define MOUSE_UP 1 << 19
#define MOUSE_DOWN 1 << 27

#define KEY_ESCAPE 27

void ui_init(void);
void ui_cleanup(void);

// Renders to buffer
void render(State *state);
// Outputs the buffer
void output(int scroll, int selection_start, int selection_end);
// Calls action associated with `ch` on element at position `y`
int invoke_action(int y, int ch, int selection);
void output_help(int scroll);

int get_screen_height(void);
int get_lines_length(void);
int get_help_length(void);
char is_line(int y);

#endif  // UI_H