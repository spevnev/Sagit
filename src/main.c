#define _XOPEN_SOURCE 700
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "action.h"
#include "error.h"
#include "git.h"
#include "state.h"
#include "ui.h"

#define KEY_ESCAPE 27

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

    if (!is_git_initialized()) ERROR("Git is not initialized in the current directory.\n");
    get_git_state(&state);
    if (is_state_empty(&state)) {
        printf("There are no changes or untracked files.\n");
        return EXIT_SUCCESS;
    }

    struct sigaction action = {0};
    action.sa_handler = stop_running;
    sigaction(SIGINT, &action, NULL);

    ui_init();
    atexit(cleanup);
    render(&state);

    char show_help = 0;
    size_t saved_scroll = 0;
    size_t scroll = 0;
    size_t cursor = 0;
    while (running) {
        clear();
        if (show_help) {
            output_help(scroll);
        } else {
            output(scroll);
            move(cursor, 0);
        }
        refresh();

        int ch = getch();
        int mouse = 0;
        if (ch == KEY_MOUSE) {
            MEVENT mouse_event = {0};
            getmouse(&mouse_event);
            mouse = mouse_event.bstate;
        }

        if (show_help) {
            if (ch == 'q' || ch == KEY_ESCAPE) {
                show_help = 0;
                scroll = saved_scroll;
                curs_set(1);
            } else if (ch == 'j' || ch == KEY_DOWN || mouse == MOUSE_DOWN) {
                if (scroll + get_screen_height() < get_help_length()) scroll++;
            } else if (ch == 'k' || ch == KEY_UP || mouse == MOUSE_UP) {
                if (scroll > 0) scroll--;
            }
        } else {
            if (ch == 'q' || ch == KEY_ESCAPE) {
                running = 0;
            } else if (ch == 'h') {
                show_help = 1;
                saved_scroll = scroll;
                scroll = 0;
                curs_set(0);
            } else if (ch == 'j' || ch == KEY_DOWN || mouse == MOUSE_DOWN) {
                size_t height = get_screen_height();
                if (cursor < height - 1) cursor++;
                else if (scroll + height < get_lines_length()) scroll++;
            } else if (ch == 'k' || ch == KEY_UP || mouse == MOUSE_UP) {
                if (cursor > 0) cursor--;
                else if (scroll > 0) scroll--;
            } else {
                int result = invoke_action(scroll + cursor, ch);
                if (result & AC_UPDATE_STATE) update_git_state(&state);
                if (result & AC_RERENDER) render(&state);
            }
        }
    }

    return EXIT_SUCCESS;
}
