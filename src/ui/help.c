#include "help.h"
#include <ncurses.h>

// clang-format off
static const char *help_lines[] = {
    "Keybindings:"                                                             ,
    "q, esc  - quit, close help"                                               ,
    "h       - open/close (this) help"                                         ,
    "j, down - next line"                                                      ,
    "k, up   - previous line"                                                  ,
    "space   - on section, file, hunk: fold/unfold"                            ,
    "          on line: start selecting a range"                               ,
    "s       - stage untracked/unstaged change"                                ,
    "u       - unstaged staged change"                                         ,
    ""                                                                         ,
    "(Un)Staging scopes:"                                                      ,
    "Files and hunks by selecting their headers,"                              ,
    "Lines and ranges within hunks."                                           ,
    ""                                                                         ,
    "Selecting a range:"                                                       ,
    "Ranges must be selected within a single hunk!"                            ,
    "To start selecting a range press space and then move cursor to select."   ,
    "When it is selected, any action will be performed on the selected area."  ,
    "To deselect, press space again or go out of hunk's scope."
};
// clang-format on

void output_help(int scroll) {
    for (int i = 0; i < getmaxy(stdscr); i++) {
        if (scroll + i >= get_help_length()) break;
        printw("%s\n", help_lines[scroll + i]);
    }
}

int get_help_length(void) { return sizeof(help_lines) / sizeof(help_lines[0]); }