#include "ui.h"
#include <assert.h>
#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "state.h"
#include "vector.h"

typedef int action_t(void *, int);  // return value indicates whether to rerender

typedef struct {
    char *str;
    action_t *action;
    void *action_arg;
} Line;

DEFINE_VECTOR_TYPE(LineVec, Line);

static LineVec lines = {0};

#define FOLD_CHAR(is_folded) ((is_folded) ? "▸" : "▾")

#define ADD_LINE(action, arg, ...)                           \
    do {                                                     \
        size_t size = snprintf(NULL, 0, __VA_ARGS__) + 1;    \
        char *str = (char *) malloc(size + 1);               \
        if (str == NULL) ERROR("Process is out of memory."); \
        snprintf(str, size, __VA_ARGS__);                    \
        str[size - 1] = '\n';                                \
        str[size] = '\0';                                    \
        Line line = {str, action, arg};                      \
        VECTOR_PUSH(&lines, line);                           \
    } while (0)

#define EMPTY_LINE() ADD_LINE(NULL, NULL, "")

#define APPEND_LINE(...)                                           \
    do {                                                           \
        assert(lines.length > 0);                                  \
        Line *line = &lines.data[lines.length - 1];                \
        size_t old_size = strlen(line->str) - 1;                   \
        size_t add_size = snprintf(NULL, 0, __VA_ARGS__) + 1;      \
        size_t new_size = old_size + add_size;                     \
        line->str = (char *) realloc(line->str, new_size + 1);     \
        if (line->str == NULL) ERROR("Process is out of memory."); \
        snprintf(line->str + old_size, add_size, __VA_ARGS__);     \
        line->str[new_size - 1] = '\n';                            \
        line->str[new_size] = '\0';                                \
    } while (0)

static int hunk_action(void *_hunk, int ch) {
    Hunk *hunk = (Hunk *) _hunk;

    if (ch == ' ') {
        hunk->is_folded ^= 1;
        return 1;
    }

    return 0;
}

static int file_action(void *_file, int ch) {
    File *file = (File *) _file;

    if (ch == ' ') {
        file->is_folded ^= 1;
        return 1;
    }

    return 0;
}

static int section_action(void *_section, int ch) {
    Section *section = (Section *) _section;

    if (ch == ' ') {
        section->is_folded ^= 1;
        return 1;
    }

    return 0;
}

static void render_files(FileVec *files) {
    for (size_t i = 0; i < files->length; i++) {
        File *file = &files->data[i];

        ADD_LINE(&file_action, file, "%s", FOLD_CHAR(file->is_folded));
        if (strcmp(file->dest, "/dev/null") == 0) {
            APPEND_LINE("deleted  %s", file->src + 2);
        } else if (strcmp(file->src + 2, file->dest + 2) != 0) {
            APPEND_LINE("renamed  %s -> %s", file->src + 2, file->dest + 2);
        } else {
            APPEND_LINE("modified %s", file->src + 2);
        }
        if (file->is_folded) continue;

        for (size_t i = 0; i < file->hunks.length; i++) {
            Hunk *hunk = &file->hunks.data[i];

            ADD_LINE(&hunk_action, hunk, "%s%s", FOLD_CHAR(hunk->is_folded), hunk->header);
            if (hunk->is_folded) continue;

            for (size_t j = 0; j < hunk->lines.length; j++) {
                ADD_LINE(NULL, NULL, "%s", hunk->lines.data[j]);
            }
        }
    }
}

void ui_init(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    start_color();
}

void ui_cleanup(void) {
    endwin();

    for (size_t i = 0; i < lines.length; i++) free(lines.data[i].str);
    VECTOR_FREE(&lines);
}

void render(State *state) {
    for (size_t i = 0; i < lines.length; i++) free(lines.data[i].str);
    lines.length = 0;

    // TODO: add colors, italics/bold

    if (state->untracked.files.length > 0) {
        ADD_LINE(&section_action, &state->untracked, "%sUntracked files:", FOLD_CHAR(state->untracked.is_folded));
        if (!state->untracked.is_folded) {
            for (size_t i = 0; i < state->untracked.files.length; i++) {
                ADD_LINE(NULL, NULL, "created %s", state->untracked.files.data[i]);
            }
        }
        EMPTY_LINE();
    }

    if (state->unstaged.files.length > 0) {
        ADD_LINE(&section_action, &state->unstaged, "%sUnstaged changes:", FOLD_CHAR(state->unstaged.is_folded));
        if (!state->unstaged.is_folded) render_files(&state->unstaged.files);
        EMPTY_LINE();
    }

    if (state->staged.files.length > 0) {
        ADD_LINE(&section_action, &state->staged, "%sStaged changes:", FOLD_CHAR(state->staged.is_folded));
        if (!state->staged.is_folded) render_files(&state->staged.files);
        EMPTY_LINE();
    }
}

void output(size_t scroll, size_t height) {
    for (size_t i = 0; i < height; i++) {
        if (scroll + i < lines.length) printw("%s", lines.data[scroll + i].str);
    }
}

void invoke_action(State *state, size_t y, int ch) {
    action_t *action = lines.data[y].action;
    if (action == NULL) return;

    int rerender = action(lines.data[y].action_arg, ch);
    if (rerender) render(state);
}

size_t get_screen_height(void) { return getmaxy(stdscr); }
size_t get_lines_length(void) { return lines.length; }