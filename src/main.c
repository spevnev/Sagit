#define _XOPEN_SOURCE 700
#include <assert.h>
#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "git.h"

static char running = 1;
static GitState git = {0};
static void render() {
    if (git.untracked.files.length > 0) {
        printw("Unstaged files:\n");

        for (size_t i = 0; i < git.untracked.files.length; i++) {
            printw("created %s\n", git.untracked.files.data[i]);
        }

        printw("\n");
    }

    if (git.unstaged.files.length > 0) {
        printw("Unstaged changes:\n");

        for (size_t i = 0; i < git.unstaged.files.length; i++) {
            File file = git.unstaged.files.data[i];

            if (strcmp(file.dest, "/dev/null") == 0) {
                printw("deleted  %s\n", file.src + 2);
            } else if (strcmp(file.src + 2, file.dest + 2) == 0) {
                printw("renamed  %s -> %s\n", file.src + 2, file.dest + 2);
            } else {
                printw("modified %s\n", file.src + 2);
            }

            for (size_t i = 0; i < file.hunks.length; i++) {
                str_vec hunk = file.hunks.data[i];
                for (size_t j = 0; j < hunk.length; j++) {
                    printw("%s\n", hunk.data[j]);
                }
            }
        }

        printw("\n");
    }

    if (git.staged.files.length > 0) {
        printw("Staged changes:\n");

        for (size_t i = 0; i < git.staged.files.length; i++) {
            File file = git.staged.files.data[i];

            if (strcmp(file.dest, "/dev/null") == 0) {
                printw("deleted  %s\n", file.src + 2);
            } else if (strcmp(file.src + 2, file.dest + 2) == 0) {
                printw("renamed  %s -> %s\n", file.src + 2, file.dest + 2);
            } else {
                printw("modified %s\n", file.src + 2);
            }

            for (size_t i = 0; i < file.hunks.length; i++) {
                str_vec hunk = file.hunks.data[i];
                for (size_t j = 0; j < hunk.length; j++) {
                    printw("%s\n", hunk.data[j]);
                }
            }
        }

        printw("\n");
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


    while (running) {
        switch (getch()) {
            case 'q':
                running = false;
                break;
            case 'h':
                assert(false);  // TODO:
                break;
            case 'j':
                assert(false);  // TODO:
                break;
            case 'k':
                assert(false);  // TODO:
                break;
            default:
                break;
        }
    }

    return EXIT_SUCCESS;
}
