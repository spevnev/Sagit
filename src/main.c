#define _XOPEN_SOURCE 700
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "git/git.h"
#include "git/state.h"
#include "ui/action.h"
#include "ui/help.h"
#include "ui/ui.h"
#include "utils/error.h"

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))

#define MIN_WIDTH 80
#define MIN_HEIGHT 10

#define SCREEN_PADDING 2

static int running = 1;
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

    int show_help = 0;
    int saved_scroll = 0;
    int scroll = 0;
    int cursor = 0;
    int selection = -1;
    while (running) {
        if (getmaxx(stdscr) < MIN_WIDTH || getmaxy(stdscr) < MIN_HEIGHT) {
            clear();
            printw("Screen is too small! Make sure it is at least %dx%d.\n", MIN_WIDTH, MIN_HEIGHT);
            printw("Press q key to exit.\n");

            int ch = getch();
            if (ch == 'q') running = 0;
            continue;
        }

        int y = scroll + cursor;
        int height = get_screen_height();

        int selection_start = -1;
        int selection_end = -1;
        if (selection != -1) {
            selection_start = MIN(selection, y);
            selection_end = MAX(selection, y);
        }

        clear();
        if (show_help) {
            output_help(scroll);
        } else {
            output(scroll, selection_start, selection_end);
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
            if (ch == 'q' || ch == KEY_ESCAPE || ch == 'h') {
                show_help = 0;
                scroll = saved_scroll;
                curs_set(1);
            } else if (ch == 'j' || ch == KEY_DOWN || mouse == MOUSE_SCROLL_DOWN) {
                if (scroll + height < get_help_length()) scroll++;
            } else if (ch == 'k' || ch == KEY_UP || mouse == MOUSE_SCROLL_UP) {
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
            } else if (ch == 'j' || ch == KEY_DOWN || mouse == MOUSE_SCROLL_DOWN) {
                if (cursor < height - 1 - SCREEN_PADDING) cursor++;
                else if (scroll + height < get_lines_length()) scroll++;
                else if (cursor < height - 1) cursor++;

                if (!is_line(scroll + cursor)) selection = -1;
            } else if (ch == 'k' || ch == KEY_UP || mouse == MOUSE_SCROLL_UP) {
                if (cursor > SCREEN_PADDING) cursor--;
                else if (scroll > 0) scroll--;
                else if (cursor > 0) cursor--;

                if (!is_line(scroll + cursor)) selection = -1;
            } else if (y < get_lines_length()) {
                int result = invoke_action(y, ch, selection_start, selection_end);
                if (result & AC_UPDATE_STATE) update_git_state(&state);
                if (result & AC_RERENDER) render(&state);
                if (result & AC_TOGGLE_SELECTION) {
                    if (selection == -1) selection = y;
                    else selection = -1;
                }
            }
        }
    }

    return EXIT_SUCCESS;
}
