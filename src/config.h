#ifndef CONFIG_H
#define CONFIG_H

#include "ui/ui.h"

#define SCROLL_PADDING 2

// clang-format off
static int styles[__LS_SIZE][3] = {
    //              foreground           background     attribute
    [LS_SECTION]  = {COLOR_WHITE,        COLOR_DEFAULT, A_BOLD},
    [LS_FILE]     = {COLOR_WHITE,        COLOR_DEFAULT, A_ITALIC},
    [LS_HUNK]     = {COLOR_BRIGHT_CYAN,  COLOR_DEFAULT, A_NONE},
    [LS_LINE]     = {COLOR_BRIGHT_WHITE, COLOR_BLACK,   A_NONE},
    [LS_ADD_LINE] = {COLOR_BRIGHT_GREEN, COLOR_BLACK,   A_NONE},
    [LS_DEL_LINE] = {COLOR_BRIGHT_RED,   COLOR_BLACK,   A_NONE},
};
// clang-format on

// NOTE: must return string
#define FOLD_CHAR(is_folded) ((is_folded) ? "▸" : "▾")

#endif  // CONFIG_H