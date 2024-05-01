#ifndef CONFIG_H
#define CONFIG_H

#include "ui/ui.h"

// number of lines before the edge of the screen when scrolling starts
#define SCROLL_PADDING 3

// clang-format off
static const int styles[__LS_SIZE][3] = {
    //              foreground           background     attribute(man curs_attr)
    [LS_SECTION]  = {COLOR_WHITE,        COLOR_DEFAULT, A_BOLD},
    [LS_FILE]     = {COLOR_WHITE,        COLOR_DEFAULT, A_ITALIC},
    [LS_HUNK]     = {COLOR_BRIGHT_CYAN,  COLOR_DEFAULT, A_NONE},
    [LS_LINE]     = {COLOR_BRIGHT_WHITE, COLOR_BLACK,   A_NONE},
    [LS_ADD_LINE] = {COLOR_BRIGHT_GREEN, COLOR_BLACK,   A_NONE},
    [LS_DEL_LINE] = {COLOR_BRIGHT_RED,   COLOR_BLACK,   A_NONE},
};
// clang-format on

// NOTE: must return string of length 1
#define FOLD_CHAR(is_folded) ((is_folded) ? "▸" : "▾")

#endif  // CONFIG_H
