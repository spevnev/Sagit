#include "ui.h"
#include <assert.h>
#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "git.h"
#include "memory.h"
#include "state.h"
#include "vector.h"

typedef int action_t(void *, int);  // return value indicates whether to rerender

typedef struct {
    char *str;
    action_t *action;
    void *action_arg;
    int style;
} Line;

DEFINE_VECTOR_TYPE(LineVec, Line);

static MemoryContext ctxt = {0};
static LineVec lines = {0};

#define FOLD_CHAR(is_folded) ((is_folded) ? "▸" : "▾")

#define ADD_LINE(action, arg, style, ...)                    \
    do {                                                     \
        size_t size = snprintf(NULL, 0, __VA_ARGS__) + 1;    \
        char *str = (char *) ctxt_alloc(&ctxt, size + 1);    \
        if (str == NULL) ERROR("Process is out of memory."); \
        snprintf(str, size, __VA_ARGS__);                    \
        str[size - 1] = '\n';                                \
        str[size] = '\0';                                    \
        Line line = {str, action, arg, style};               \
        VECTOR_PUSH(&lines, line);                           \
    } while (0)

#define EMPTY_LINE()                                         \
    do {                                                     \
        char *str = (char *) ctxt_alloc(&ctxt, 2);           \
        if (str == NULL) ERROR("Process is out of memory."); \
        str[0] = '\n';                                       \
        str[1] = '\0';                                       \
        Line line = {str, NULL, NULL, 0};                    \
        VECTOR_PUSH(&lines, line);                           \
    } while (0)

#define APPEND_LINE(...)                                                   \
    do {                                                                   \
        assert(lines.length > 0);                                          \
        Line *line = &lines.data[lines.length - 1];                        \
        size_t old_size = strlen(line->str) - 1;                           \
        size_t add_size = snprintf(NULL, 0, __VA_ARGS__) + 1;              \
        size_t new_size = old_size + add_size;                             \
        line->str = (char *) ctxt_realloc(&ctxt, line->str, new_size + 1); \
        if (line->str == NULL) ERROR("Process is out of memory.");         \
        snprintf(line->str + old_size, add_size, __VA_ARGS__);             \
        line->str[new_size - 1] = '\n';                                    \
        line->str[new_size] = '\0';                                        \
    } while (0)

static int hunk_action(void *_hunk, int ch) {
    Hunk *hunk = (Hunk *) _hunk;

    if (ch == ' ') {
        hunk->is_folded ^= 1;
        return AC_RERENDER;
    }

    return 0;
}

static int file_action(void *_file, int ch) {
    File *file = (File *) _file;

    if (ch == ' ') {
        file->is_folded ^= 1;
        return AC_RERENDER;
    } else if (ch == 'u') {
        // TODO: check that it is staged
        git_unstage_file(file->dest + 2);
        return AC_RERENDER | AC_REFRESH_STATE;
    }

    return 0;
}

static int untracked_file_action(void *_file, int ch) {
    char *file = (char *) _file;

    if (ch == 's') {
        git_stage_file(file);
        return AC_RERENDER | AC_REFRESH_STATE;
    }

    return 0;
}

static int section_action(void *_section, int ch) {
    Section *section = (Section *) _section;

    if (ch == ' ') {
        section->is_folded ^= 1;
        return AC_RERENDER;
    }

    return 0;
}

static int section_style = 0;
static int file_style = 0;
static int hunk_style = 0;
static int def_line_style = 0;
static int add_line_style = 0;
static int del_line_style = 0;

static void init_styles(void) {
    short pair_num = 0;

    init_pair(++pair_num, 0, 0);
    section_style = COLOR_PAIR(pair_num);

    init_pair(++pair_num, 0, 0);
    file_style = COLOR_PAIR(pair_num);

    init_pair(++pair_num, 0, 0);
    hunk_style = COLOR_PAIR(pair_num);

    init_pair(++pair_num, 0, 0);
    def_line_style = COLOR_PAIR(pair_num);

    init_pair(++pair_num, 0, 0);
    add_line_style = COLOR_PAIR(pair_num);

    init_pair(++pair_num, 0, 0);
    del_line_style = COLOR_PAIR(pair_num);
}

static void render_files(FileVec *files) {
    assert(files != NULL);

    for (size_t i = 0; i < files->length; i++) {
        File *file = &files->data[i];

        ADD_LINE(&file_action, file, file_style, "%s", FOLD_CHAR(file->is_folded));
        if (strcmp(file->src + 2, file->dest + 2) == 0) {
            APPEND_LINE("modified %s", file->src + 2);
        } else if (strcmp(file->dest, "/dev/null") == 0) {
            APPEND_LINE("deleted  %s", file->src + 2);
        } else if (strcmp(file->src, "/dev/null") == 0) {
            APPEND_LINE("created  %s", file->dest + 2);
        } else {
            APPEND_LINE("renamed  %s -> %s", file->src + 2, file->dest + 2);
        }
        if (file->is_folded) continue;

        for (size_t i = 0; i < file->hunks.length; i++) {
            Hunk *hunk = &file->hunks.data[i];

            ADD_LINE(&hunk_action, hunk, hunk_style, "%s%s", FOLD_CHAR(hunk->is_folded), hunk->header);
            if (hunk->is_folded) continue;

            for (size_t j = 0; j < hunk->lines.length; j++) {
                char ch = hunk->lines.data[j][0];
                int style = def_line_style;
                if (ch == '+') style = add_line_style;
                if (ch == '-') style = del_line_style;
                ADD_LINE(NULL, NULL, style, "%s", hunk->lines.data[j]);
            }
        }
    }
}

void ui_init(void) {
    ctxt_init(&ctxt);

    setlocale(LC_ALL, "");
    initscr();

    cbreak();
    noecho();
    set_escdelay(0);
    keypad(stdscr, true);
    mousemask(MOUSE_UP | MOUSE_DOWN, NULL);

    start_color();
    init_styles();
}

void ui_cleanup(void) {
    endwin();

    ctxt_free(&ctxt);
    VECTOR_FREE(&lines);
}

void render(State *state) {
    assert(state != NULL);

    ctxt_reset(&ctxt);
    VECTOR_CLEAR(&lines);

    if (state->untracked.files.length > 0) {
        ADD_LINE(&section_action, &state->untracked, section_style, "%sUntracked files:", FOLD_CHAR(state->untracked.is_folded));
        if (!state->untracked.is_folded) {
            for (size_t i = 0; i < state->untracked.files.length; i++) {
                char *file_path = state->untracked.files.data[i];
                ADD_LINE(&untracked_file_action, file_path, file_style, "created %s", file_path);
            }
        }
        EMPTY_LINE();
    }

    if (state->unstaged.files.length > 0) {
        ADD_LINE(&section_action, &state->unstaged, section_style, "%sUnstaged changes:", FOLD_CHAR(state->unstaged.is_folded));
        if (!state->unstaged.is_folded) render_files(&state->unstaged.files);
        EMPTY_LINE();
    }

    if (state->staged.files.length > 0) {
        ADD_LINE(&section_action, &state->staged, section_style, "%sStaged changes:", FOLD_CHAR(state->staged.is_folded));
        if (!state->staged.is_folded) render_files(&state->staged.files);
        EMPTY_LINE();
    }
}

void output(size_t scroll) {
    for (int i = 0; i < getmaxy(stdscr); i++) {
        if (scroll + i >= lines.length) break;

        Line line = lines.data[scroll + i];
        attron(line.style);
        printw("%s", line.str);
        attroff(line.style);
    }
}

int invoke_action(size_t y, int ch) {
    action_t *action = lines.data[y].action;
    if (action != NULL) return action(lines.data[y].action_arg, ch);
    return 0;
}

// clang-format off
static const char *help_lines[] = {
    "Help page. Keybindings:"                                ,
    "q, esc        - quit, close help"                       ,
    "h             - show this page"                         ,
    "j, down arrow - next line"                              ,
    "k, up arrow   - previous line"                          ,
    "f, space      - (un)fold block"                         ,
    "s             - stage untracked file or unstaged change",
    "u             - unstaged staged change"
};
// clang-format on

void output_help(size_t scroll) {
    for (int i = 0; i < getmaxy(stdscr); i++) {
        if (scroll + i >= get_help_length()) break;
        printw("%s\n", help_lines[scroll + i]);
    }
}

size_t get_screen_height(void) { return getmaxy(stdscr); }
size_t get_lines_length(void) { return lines.length; }
size_t get_help_length(void) { return sizeof(help_lines) / sizeof(help_lines[0]); }