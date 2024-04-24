#ifndef UI_H
#define UI_H

#include <ncurses.h>
#include <stdlib.h>
#include "git/state.h"

// Missing ncurses definitions:

// clang-format off
#define COLOR_BRIGHT_BLACK   8
#define COLOR_BRIGHT_RED     9
#define COLOR_BRIGHT_GREEN   10
#define COLOR_BRIGHT_YELLOW  11
#define COLOR_BRIGHT_BLUE    12
#define COLOR_BRIGHT_MAGENTA 13
#define COLOR_BRIGHT_CYAN    14
#define COLOR_BRIGHT_WHITE   15
// clang-format on

#define COLOR_GREY COLOR_BRIGHT_BLACK

#define COLOR_DEFAULT -1
#define A_NONE 0

#define KEY_ESCAPE 27

#if NCURSES_MOUSE_VERSION > 1
    #define MOUSE_SCROLL_DOWN BUTTON5_PRESSED
    #define MOUSE_SCROLL_UP BUTTON4_PRESSED
#else
    #define MOUSE_SCROLL_DOWN 1 << 27
    #define MOUSE_SCROLL_UP 1 << 19
#endif

typedef enum { LS_SECTION, LS_FILE, LS_HUNK, LS_LINE, LS_ADD_LINE, LS_DEL_LINE, __LS_SIZE } LineStyle;

void ui_init(void);
void ui_cleanup(void);

// Renders to buffer and populates hunk_idxs
void render(State *state, size_vec *hunk_idxs);
// Outputs the buffer, returns number of wrapped lines before cursor
int output(int scroll, int cursor, int selection_start, int selection_end);
// Calls action associated with `ch` on element at position `y`
int invoke_action(int y, int ch, int range_start, int range_end);

int get_lines_length(void);
bool is_selectable(int y);

#endif  // UI_H