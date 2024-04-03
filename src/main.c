#define _XOPEN_SOURCE 700
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "git.h"
#include "state.h"
#include "ui.h"

static char running = 1;
static State state = {0};

static void cleanup(void) {
    ui_cleanup();
    free_state(&state);
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

    struct sigaction action = {0};
    action.sa_handler = stop_running;
    sigaction(SIGINT, &action, NULL);

    if (!is_git_initialized()) ERROR("Git is not initialized in the current directory.\n");
    if (get_git_state(&state)) ERROR("Unable to get git state.\n");
    if (is_state_empty(&state)) {
        printf("There are no changes or untracked files.\n");
        return EXIT_SUCCESS;
    }

    ui_init();
    atexit(cleanup);
    render(&state);

    size_t height = get_screen_height();
    size_t scroll = 0;
    size_t cursor = 0;
    while (running) {
        clear();
        output(scroll, height);
        move(cursor, 0);
        refresh();

        int ch = getch();
        switch (ch) {
            case 'q':
                running = 0;
                break;
            case 'h':
                // TODO:
                break;
            case 'j':
                if (cursor < height - 1) cursor++;
                else if (scroll + height < get_lines_length()) scroll++;
                break;
            case 'k':
                if (cursor > 0) cursor--;
                else if (scroll > 0) scroll--;
                break;
            default:
                invoke_action(&state, scroll + cursor, ch);
                break;
        }
    }

    return EXIT_SUCCESS;
}
