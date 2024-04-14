#include "ui.h"
#include <assert.h>
#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "git/git.h"
#include "git/state.h"
#include "ui/action.h"
#include "utils/error.h"
#include "utils/memory.h"
#include "utils/vector.h"

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

#define ADD_LINE(action, arg, style, ...)                 \
    do {                                                  \
        size_t size = snprintf(NULL, 0, __VA_ARGS__) + 1; \
        char *str = (char *) ctxt_alloc(&ctxt, size);     \
        snprintf(str, size, __VA_ARGS__);                 \
        Line line = {str, action, arg, style};            \
        VECTOR_PUSH(&lines, line);                        \
    } while (0)

#define EMPTY_LINE()                               \
    do {                                           \
        char *str = (char *) ctxt_alloc(&ctxt, 1); \
        str[0] = '\0';                             \
        Line line = {str, NULL, NULL, 0};          \
        VECTOR_PUSH(&lines, line);                 \
    } while (0)

#define APPEND_LINE(...)                                               \
    do {                                                               \
        assert(lines.length > 0);                                      \
        Line *line = &lines.data[lines.length - 1];                    \
        size_t old_size = strlen(line->str);                           \
        size_t add_size = snprintf(NULL, 0, __VA_ARGS__) + 1;          \
        size_t new_size = old_size + add_size;                         \
        line->str = (char *) ctxt_realloc(&ctxt, line->str, new_size); \
        snprintf(line->str + old_size, add_size, __VA_ARGS__);         \
    } while (0)

static int section_style = 0;
static int file_style = 0;
static int hunk_style = 0;
static int line_style = 0;
static int add_line_style = 0;
static int del_line_style = 0;

static void init_styles(void) {
    short pair_num = 0;

    init_pair(++pair_num, COLOR_WHITE, COLOR_DEFAULT);
    section_style = COLOR_PAIR(pair_num) | A_BOLD;

    init_pair(++pair_num, COLOR_WHITE, COLOR_DEFAULT);
    file_style = COLOR_PAIR(pair_num) | A_ITALIC;

    init_pair(++pair_num, BRIGHT(COLOR_CYAN), COLOR_DEFAULT);
    hunk_style = COLOR_PAIR(pair_num);

    short line_bg = COLOR_BLACK;
    init_pair(++pair_num, BRIGHT(COLOR_WHITE), line_bg);
    line_style = COLOR_PAIR(pair_num);

    init_pair(++pair_num, BRIGHT(COLOR_GREEN), line_bg);
    add_line_style = COLOR_PAIR(pair_num);

    init_pair(++pair_num, BRIGHT(COLOR_RED), line_bg);
    del_line_style = COLOR_PAIR(pair_num);
}

static void render_files(const FileVec *files, char staged) {
    assert(files != NULL);
    action_t *file_action = staged ? &staged_file_action : &unstaged_file_action;
    action_t *hunk_action = staged ? &staged_hunk_action : &unstaged_hunk_action;
    action_t *line_action = staged ? &staged_line_action : &unstaged_line_action;

    for (size_t i = 0; i < files->length; i++) {
        File *file = &files->data[i];

        ADD_LINE(file_action, file, file_style, "%s", FOLD_CHAR(file->is_folded));
        if (file->change_type == FC_MODIFIED) APPEND_LINE("modified %s", file->src + 2);
        else if (file->change_type == FC_DELETED) APPEND_LINE("deleted %s", file->src + 2);
        else if (file->change_type == FC_CREATED) APPEND_LINE("created %s", file->dest + 2);
        else if (file->change_type == FC_RENAMED) APPEND_LINE("renamed %s -> %s", file->src + 2, file->dest + 2);
        else ERROR("Unkown file change type.\n");

        if (file->is_folded) continue;

        for (size_t i = 0; i < file->hunks.length; i++) {
            int hunk_y = lines.length + 1;
            Hunk *hunk = &file->hunks.data[i];
            HunkArgs *args = (HunkArgs *) ctxt_alloc(&ctxt, sizeof(HunkArgs));
            args->file = file;
            args->hunk = hunk;

            ADD_LINE(hunk_action, args, hunk_style, "%s%s", FOLD_CHAR(hunk->is_folded), hunk->header);
            if (hunk->is_folded) continue;

            for (size_t j = 0; j < hunk->lines.length; j++) {
                char ch = hunk->lines.data[j][0];
                int style = line_style;
                if (ch == '+') style = add_line_style;
                if (ch == '-') style = del_line_style;
                LineArgs *args = (LineArgs *) ctxt_alloc(&ctxt, sizeof(LineArgs));
                args->file = file;
                args->hunk = hunk;
                args->hunk_y = hunk_y;
                args->line = j;
                ADD_LINE(line_action, args, style, "%s", hunk->lines.data[j]);
            }
        }
    }
}

void ui_init(void) {
    ctxt_init(&ctxt);

    setlocale(LC_ALL, "");

    if (initscr() == NULL) ERROR("Unable to initialize ncurses.\n");
    cbreak();
    noecho();
    set_escdelay(0);
    keypad(stdscr, true);
    mousemask(MOUSE_SCROLL_DOWN | MOUSE_SCROLL_UP, NULL);

#ifdef __linux__
    // only needs to be non-blocking when inotify is used
    nodelay(stdscr, true);
#endif

    start_color();
    use_default_colors();
    init_styles();
    curs_set(0);
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
        if (!state->unstaged.is_folded) render_files(&state->unstaged.files, 0);
        EMPTY_LINE();
    }

    if (state->staged.files.length > 0) {
        ADD_LINE(&section_action, &state->staged, section_style, "%sStaged changes:", FOLD_CHAR(state->staged.is_folded));
        if (!state->staged.is_folded) render_files(&state->staged.files, 1);
        EMPTY_LINE();
    }
}

void output(int scroll, int cursor, int selection_start, int selection_end) {
    for (int i = 0; i < getmaxy(stdscr); i++) {
        int y = scroll + i;
        if ((size_t) y >= lines.length) break;

        char is_selected = i == cursor || (y >= selection_start && y <= selection_end);
        int selected_style = is_selected ? A_REVERSE : 0;

        Line line = lines.data[scroll + i];
        bkgdset(line.style);
        attron(line.style | selected_style);
        printw("%s\n", line.str);
        attrset(0);
        bkgdset(0);
    }
}

int invoke_action(int y, int ch, int range_start, int range_end) {
    ActionArgs args = {ch, range_start, range_end};
    action_t *action = lines.data[y].action;
    if (action != NULL) return action(lines.data[y].action_arg, &args);
    return 0;
}

int get_screen_height(void) { return getmaxy(stdscr); }
int get_lines_length(void) { return lines.length; }
int is_line(int y) {
    // TODO: find a proper way...
    if ((size_t) y >= lines.length) return 0;
    return lines.data[y].action == &unstaged_line_action || lines.data[y].action == &staged_line_action;
}