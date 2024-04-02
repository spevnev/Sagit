#define _XOPEN_SOURCE 700
#include <assert.h>
#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "git.h"

typedef int action_t(void *, int);
DEFINE_VECTOR_TYPE(action_vec, action_t *);

static char running = 1;
static GitState git = {0};
static str_vec lines = {0};
static action_vec actions = {0};
static void_vec action_args = {0};

#define ADD_LINE(action, arg, ...)                              \
    do {                                                        \
        VECTOR_PUSH(&actions, action);                          \
        VECTOR_PUSH(&action_args, arg);                         \
        size_t size = snprintf(NULL, 0, __VA_ARGS__) + 1;       \
        char *buffer = (char *) malloc(size);                   \
        if (buffer == NULL) ERROR("Process is out of memory."); \
        snprintf(buffer, size, __VA_ARGS__);                    \
        VECTOR_PUSH(&lines, buffer);                            \
    } while (0)

static void render() {
    for (size_t i = 0; i < lines.length; i++) free(lines.data[i]);
    lines.length = 0;
    actions.length = 0;

    // TODO: add colors

    if (git.untracked.files.length > 0) {
        ADD_LINE(NULL, NULL, "Unstaged files:\n");

        for (size_t i = 0; i < git.untracked.files.length; i++) {
            ADD_LINE(NULL, NULL, "created %s\n", git.untracked.files.data[i]);
        }

        ADD_LINE(NULL, NULL, "\n");
    }

    if (git.unstaged.files.length > 0) {
        ADD_LINE(NULL, NULL, "Unstaged changes:\n");

        for (size_t i = 0; i < git.unstaged.files.length; i++) {
            File file = git.unstaged.files.data[i];

            if (strcmp(file.dest, "/dev/null") == 0) {
                ADD_LINE(NULL, NULL, "deleted  %s\n", file.src + 2);
            } else if (strcmp(file.src + 2, file.dest + 2) == 0) {
                ADD_LINE(NULL, NULL, "renamed  %s -> %s\n", file.src + 2, file.dest + 2);
            } else {
                ADD_LINE(NULL, NULL, "modified %s\n", file.src + 2);
            }

            for (size_t i = 0; i < file.hunks.length; i++) {
                str_vec *hunk = &file.hunks.data[i];

                ADD_LINE(NULL, NULL, "%s\n", hunk->data[0]);
                for (size_t j = 1; j < hunk->length; j++) {
                    ADD_LINE(NULL, NULL, "%s\n", hunk->data[j]);
                }
            }
        }

        ADD_LINE(NULL, NULL, "\n");
    }

    if (git.staged.files.length > 0) {
        ADD_LINE(NULL, NULL, "Staged changes:\n");

        for (size_t i = 0; i < git.staged.files.length; i++) {
            File file = git.staged.files.data[i];

            if (strcmp(file.dest, "/dev/null") == 0) {
                ADD_LINE(NULL, NULL, "deleted  %s\n", file.src + 2);
            } else if (strcmp(file.src + 2, file.dest + 2) == 0) {
                ADD_LINE(NULL, NULL, "renamed  %s -> %s\n", file.src + 2, file.dest + 2);
            } else {
                ADD_LINE(NULL, NULL, "modified %s\n", file.src + 2);
            }

            for (size_t i = 0; i < file.hunks.length; i++) {
                str_vec *hunk = &file.hunks.data[i];

                ADD_LINE(NULL, NULL, "%s\n", hunk->data[0]);
                for (size_t j = 1; j < hunk->length; j++) {
                    ADD_LINE(NULL, NULL, "%s\n", hunk->data[j]);
                }
            }
        }

        ADD_LINE(NULL, NULL, "\n");
    }
}

static void cleanup() {
    endwin();

    free(git.untracked.raw);
    VECTOR_FREE(&git.untracked.files);
    free(git.unstaged.raw);
    free_files(&git.unstaged.files);
    free(git.staged.raw);
    free_files(&git.staged.files);

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
    if (get_git_state(&git)) ERROR("Unable to get git state.\n");
    render(&git);

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
