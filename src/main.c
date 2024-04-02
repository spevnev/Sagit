#define _XOPEN_SOURCE 700
#include <assert.h>
#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "git.h"

typedef int action_t(void *, int);  // return value indicates whether to rerender

DEFINE_VECTOR_TYPE(action_vec, action_t *);

static char running = 1;
static State state = {0};
static str_vec lines = {0};
static action_vec actions = {0};
static void_vec action_args = {0};

#define FOLD_CHAR(is_folded) ((is_folded) ? "▸" : "▾")

#define ADD_LINE(action, arg, ...)                            \
    do {                                                      \
        VECTOR_PUSH(&actions, action);                        \
        VECTOR_PUSH(&action_args, arg);                       \
        size_t size = snprintf(NULL, 0, __VA_ARGS__) + 1;     \
        char *line = (char *) malloc(size);                   \
        if (line == NULL) ERROR("Process is out of memory."); \
        snprintf(line, size, __VA_ARGS__);                    \
        VECTOR_PUSH(&lines, line);                            \
    } while (0)

#define APPEND_LINE(...)                                                                       \
    do {                                                                                       \
        assert(lines.length > 0);                                                              \
        char *old_line = lines.data[lines.length - 1];                                         \
        size_t old_size = strlen(old_line);                                                    \
        size_t additional_size = snprintf(NULL, 0, __VA_ARGS__) + 1;                           \
        lines.data[lines.length - 1] = (char *) realloc(old_line, old_size + additional_size); \
        if (lines.data[lines.length - 1] == NULL) ERROR("Process is out of memory.");          \
        snprintf(lines.data[lines.length - 1] + old_size - 1, additional_size, __VA_ARGS__);   \
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

        ADD_LINE(&file_action, file, "%s\n", FOLD_CHAR(file->is_folded));
        if (strcmp(file->dest, "/dev/null") == 0) {
            APPEND_LINE("deleted  %s\n", file->src + 2);
        } else if (strcmp(file->src + 2, file->dest + 2) != 0) {
            APPEND_LINE("renamed  %s -> %s\n", file->src + 2, file->dest + 2);
        } else {
            APPEND_LINE("modified %s\n", file->src + 2);
        }
        if (file->is_folded) continue;

        for (size_t i = 0; i < file->hunks.length; i++) {
            Hunk *hunk = &file->hunks.data[i];

            ADD_LINE(&hunk_action, hunk, "%s%s\n", FOLD_CHAR(hunk->is_folded), hunk->header);
            if (hunk->is_folded) continue;

            for (size_t j = 0; j < hunk->lines.length; j++) {
                ADD_LINE(NULL, NULL, "%s\n", hunk->lines.data[j]);
            }
        }
    }
}

static void render(void) {
    for (size_t i = 0; i < lines.length; i++) free(lines.data[i]);
    lines.length = 0;
    actions.length = 0;
    action_args.length = 0;

    // TODO: add colors, italics/bold

    if (state.untracked.files.length > 0) {
        ADD_LINE(&section_action, &state.untracked, "%sUntracked files:\n", FOLD_CHAR(state.untracked.is_folded));
        if (!state.untracked.is_folded) {
            for (size_t i = 0; i < state.untracked.files.length; i++) {
                ADD_LINE(NULL, NULL, "created %s\n", state.untracked.files.data[i]);
            }
        }
        ADD_LINE(NULL, NULL, "\n");
    }

    if (state.unstaged.files.length > 0) {
        ADD_LINE(&section_action, &state.unstaged, "%sUnstaged files:\n", FOLD_CHAR(state.unstaged.is_folded));
        if (!state.unstaged.is_folded) render_files(&state.unstaged.files);
        ADD_LINE(NULL, NULL, "\n");
    }

    if (state.staged.files.length > 0) {
        ADD_LINE(&section_action, &state.staged, "%sStaged files:\n", FOLD_CHAR(state.staged.is_folded));
        if (!state.staged.is_folded) render_files(&state.staged.files);
        ADD_LINE(NULL, NULL, "\n");
    }
}

static void cleanup(void) {
    endwin();

    free(state.untracked.raw);
    VECTOR_FREE(&state.untracked.files);
    free(state.unstaged.raw);
    free_files(&state.unstaged.files);
    free(state.staged.raw);
    free_files(&state.staged.files);

    for (size_t i = 0; i < lines.length; i++) free(lines.data[i]);

    VECTOR_FREE(&lines);
    VECTOR_FREE(&actions);
    VECTOR_FREE(&action_args);
}

static void stop_running(int signal) {
    (void) signal;
    running = 0;
}

int main(int argc, char **argv) {
    if (argc > 1) {
        fprintf(stderr, "usage: %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!is_git_initialized()) ERROR("Git is not initialized in the current directory.\n");
    if (get_git_state(&state)) ERROR("Unable to get git state.\n");
    render();

    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    start_color();

    atexit(cleanup);

    struct sigaction action = {0};
    action.sa_handler = stop_running;
    sigaction(SIGINT, &action, NULL);

    size_t scroll = 0;
    size_t cursor = 0;

    while (running) {
        size_t height = getmaxy(stdscr);

        clear();
        for (size_t i = 0; i < height; i++) {
            if (scroll + i < lines.length) printw("%s", lines.data[scroll + i]);
        }
        move(cursor, 0);
        refresh();

        int ch = getch();
        switch (ch) {
            case 'q':
                running = false;
                break;
            case 'h':
                assert(false);  // TODO:
                break;
            case 'j':
                if (cursor < height - 1) cursor++;
                else if (scroll + height - 1 < lines.length) scroll++;
                break;
            case 'k':
                if (cursor > 0) cursor--;
                else if (scroll > 0) scroll--;
                break;
            default: {
                int i = scroll + cursor;
                action_t *action = actions.data[i];
                if (action != NULL) {
                    int rerender = action(action_args.data[i], ch);
                    if (rerender) render();
                }
            } break;
        }
    }

    return EXIT_SUCCESS;
}
