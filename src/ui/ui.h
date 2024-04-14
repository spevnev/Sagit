#ifndef UI_H
#define UI_H

#include <ncurses.h>
#include <stdlib.h>
#include "git/state.h"

// Missing ncurses definitions:
#define COLOR_DEFAULT -1
#define BRIGHT(color) (COLORS > 15 ? (color + 8) : color)
#define KEY_ESCAPE 27

#if NCURSES_MOUSE_VERSION > 1
#    define MOUSE_SCROLL_DOWN BUTTON5_PRESSED
#    define MOUSE_SCROLL_UP BUTTON4_PRESSED
#else
#    define MOUSE_SCROLL_DOWN 1 << 27
#    define MOUSE_SCROLL_UP 1 << 19
#endif

void ui_init(void);
void ui_cleanup(void);

// Renders to buffer
void render(State *state);
// Outputs the buffer
void output(int scroll, int cursor, int selection_start, int selection_end);
// Calls action associated with `ch` on element at position `y`
int invoke_action(int y, int ch, int range_start, int range_end);

int get_screen_height(void);
int get_lines_length(void);
int is_selectable(int y);

#endif  // UI_H