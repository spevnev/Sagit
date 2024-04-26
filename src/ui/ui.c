#include "ui.h"
#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "error.h"
#include "git/git.h"
#include "git/state.h"
#include "memory.h"
#include "ui/action.h"
#include "ui/sort.h"
#include "vector.h"

typedef struct {
    char *str;
    action_t *action;
    void *action_arg;
    int style;
    bool is_selectable;
} Line;

VECTOR_TYPEDEF(LineVec, Line);

static MemoryContext ctxt = {0};
static LineVec lines = {0};
static int line_styles[__LS_SIZE] = {0};
static int_vec hunk_indexes = {0};

#define ADD_LINE(action, arg, style, is_selectable, ...)                   \
    do {                                                                   \
        size_t size = snprintf(NULL, 0, __VA_ARGS__) + 1;                  \
        char *str = (char *) ctxt_alloc(&ctxt, size);                      \
        snprintf(str, size, __VA_ARGS__);                                  \
        Line line = {str, action, arg, line_styles[style], is_selectable}; \
        VECTOR_PUSH(&lines, line);                                         \
    } while (0)

#define EMPTY_LINE()                                       \
    do {                                                   \
        char *str = (char *) ctxt_alloc(&ctxt, 2);         \
        str[0] = ' '; /* makes it visible when selected */ \
        str[1] = '\0';                                     \
        Line line = {str, NULL, NULL, 0, 0};               \
        VECTOR_PUSH(&lines, line);                         \
    } while (0)

#define APPEND_LINE(...)                                               \
    do {                                                               \
        ASSERT(lines.length > 0);                                      \
        Line *line = &lines.data[lines.length - 1];                    \
        size_t old_size = strlen(line->str);                           \
        size_t add_size = snprintf(NULL, 0, __VA_ARGS__) + 1;          \
        size_t new_size = old_size + add_size;                         \
        line->str = (char *) ctxt_realloc(&ctxt, line->str, new_size); \
        snprintf(line->str + old_size, add_size, __VA_ARGS__);         \
    } while (0)

static void init_styles(void) {
    ASSERT(sizeof(line_styles) / sizeof(line_styles[0]) == __LS_SIZE);

    for (int i = 0, pair_num = 1; i < __LS_SIZE; i++, pair_num++) {
        ASSERT(styles[i][0] < COLORS && styles[i][1] < COLORS && pair_num < COLOR_PAIRS);

        init_pair(pair_num, styles[i][0], styles[i][1]);
        line_styles[i] = COLOR_PAIR(pair_num) | styles[i][2];
    }
}

static void render_files(const FileVec *files, action_t *file_action, action_t *hunk_action, action_t *line_action) {
    ASSERT(files != NULL);

    size_vec sorted_indexes = sort_files(files);
    for (size_t i = 0; i < sorted_indexes.length; i++) {
        File *file = &files->data[sorted_indexes.data[i]];

        if (file->change_type == FC_DELETED) {
            // Don't display deleted files' content
            VECTOR_PUSH(&hunk_indexes, lines.length);
            ADD_LINE(file_action, file, LS_FILE, false, " deleted %s", file->src);
            continue;
        }

        ADD_LINE(file_action, file, LS_FILE, false, "%s", FOLD_CHAR(file->is_folded));
        switch (file->change_type) {
            case FC_MODIFIED:
                APPEND_LINE("modified %s", file->src);
                break;
            case FC_CREATED:
                APPEND_LINE("created %s", file->dst);
                break;
            case FC_RENAMED:
                APPEND_LINE("renamed %s -> %s", file->src, file->dst);
                break;
            default:
                UNREACHABLE();
        }

        if (file->is_folded || file->change_type == FC_CREATED) VECTOR_PUSH(&hunk_indexes, lines.length - 1);

        if (file->is_folded) continue;

        if (file->is_binary) {
            ADD_LINE(NULL, NULL, LS_LINE, false, "<binary file>");
            continue;
        }

        if (file->change_type == FC_CREATED && file->hunks.length == 0) {
            ADD_LINE(NULL, NULL, LS_LINE, false, "<empty file>");
            continue;
        }

        for (size_t i = 0; i < file->hunks.length; i++) {
            Hunk *hunk = &file->hunks.data[i];

            HunkArgs *args = (HunkArgs *) ctxt_alloc(&ctxt, sizeof(HunkArgs));
            args->file = file;
            args->hunk = hunk;
            if (file->change_type != FC_CREATED) {
                // Created files always have one hunk, so there is no need to render it
                VECTOR_PUSH(&hunk_indexes, lines.length);
                ADD_LINE(hunk_action, args, LS_HUNK, false, "%s%s", FOLD_CHAR(hunk->is_folded), hunk->header);
                if (hunk->is_folded) continue;
            }

            int hunk_y = lines.length;
            for (size_t j = 0; j < hunk->lines.length; j++) {
                char ch = hunk->lines.data[j][0];

                LineStyle style = LS_LINE;
                if (ch == '+') style = LS_ADD_LINE;
                if (ch == '-') style = LS_DEL_LINE;

                LineArgs *args = (LineArgs *) ctxt_alloc(&ctxt, sizeof(LineArgs));
                args->file = file;
                args->hunk = hunk;
                args->hunk_y = hunk_y;
                args->line = j;

                ADD_LINE(line_action, args, style, true, "%s", hunk->lines.data[j]);
            }
        }
    }

    VECTOR_FREE(&sorted_indexes);
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
    nodelay(stdscr, true);

    start_color();
    use_default_colors();
    init_styles();
    curs_set(0);
}

void ui_cleanup(void) {
    endwin();

    ctxt_free(&ctxt);
    VECTOR_FREE(&lines);
    VECTOR_FREE(&hunk_indexes);
}

void render(State *state) {
    ASSERT(state != NULL);

    ctxt_reset(&ctxt);
    VECTOR_RESET(&lines);
    VECTOR_RESET(&hunk_indexes);

    if (state->unstaged.files.length > 0) {
        ADD_LINE(&section_action, &state->unstaged, LS_SECTION, 0, "%sUnstaged changes:", FOLD_CHAR(state->unstaged.is_folded));
        if (!state->unstaged.is_folded) render_files(&state->unstaged.files, &unstaged_file_action, &unstaged_hunk_action, &unstaged_line_action);
        EMPTY_LINE();
    }

    if (state->staged.files.length > 0) {
        ADD_LINE(&section_action, &state->staged, LS_SECTION, 0, "%sStaged changes:", FOLD_CHAR(state->staged.is_folded));
        if (!state->staged.is_folded) render_files(&state->staged.files, &staged_file_action, &staged_hunk_action, &staged_line_action);
        EMPTY_LINE();
    }
}

int output(int scroll, int cursor, int selection_start, int selection_end) {
    int width = getmaxx(stdscr), height = getmaxy(stdscr);
    int wrapped = 0, wrapped_before_cursor = 0;

    for (int i = 0; i < height - wrapped; i++) {
        int y = scroll + i;
        if ((size_t) y >= lines.length) break;

        bool is_selected = i == cursor || (y >= selection_start && y <= selection_end);
        Line line = lines.data[scroll + i];
        int length = strlen(line.str);

        attrset(line.style | (is_selected ? A_REVERSE : 0));
        bkgdset(line.style);
        printw("%s", line.str);
        if (length % width != 0) printw("\n");

        wrapped += (length - 1) / width;
        if (i <= cursor) wrapped_before_cursor = wrapped;
    }

    for (int i = lines.length - scroll; i < height; i++) {
        attrset(i == cursor ? A_REVERSE : 0);
        printw(" \n");
    }

    clrtobot();

    attrset(0);
    bkgdset(0);
    return wrapped_before_cursor;
}

int invoke_action(int y, int ch, int range_start, int range_end) {
    action_t *action = lines.data[y].action;
    if (action == NULL) return 0;

    ActionArgs args = {ch, range_start, range_end};
    return action(lines.data[y].action_arg, &args);
}

int get_prev_hunk(int y) {
    int hunk_y = -1;
    for (size_t i = 0; i < hunk_indexes.length; i++) {
        if (hunk_indexes.data[i] >= y) break;
        hunk_y = hunk_indexes.data[i];
    }

    if (hunk_y >= y) return -1;
    return hunk_y;
}

int get_next_hunk(int y) {
    for (size_t i = 0; i < hunk_indexes.length; i++) {
        if (hunk_indexes.data[i] > y) return hunk_indexes.data[i];
    }

    return -1;
}

int get_lines_length(void) { return lines.length; }
bool is_selectable(int y) { return (size_t) y < lines.length && lines.data[y].is_selectable; }