#include <ncurses.h>
#include <stdio.h>
#include "config.h"
#include "event.h"
#include "git/git.h"
#include "git/state.h"
#include "signals.h"
#include "ui/action.h"
#include "ui/help.h"
#include "ui/ui.h"
#include "utils/error.h"
#include "utils/vector.h"

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))

#define MIN_WIDTH 80
#define MIN_HEIGHT 10

static bool show_help = false;
static State state = {0};

static void cleanup(void) {
    poll_cleanup();
    ui_cleanup();
    free_state(&state);
}


void handle_info(void) {
    refresh();

    if (!poll_events(&state)) return;

    int ch;
    while ((ch = getch()) != ERR) {
        if (ch == 'q' || ch == KEY_ESCAPE) running = 0;
    }
}

void handle_help(void) {
    static int help_scroll = 0;

    output_help(help_scroll);
    refresh();

    if (!poll_events(&state)) return;

    int ch;
    while ((ch = getch()) != ERR) {
        if (ch == KEY_MOUSE) {
            MEVENT mouse_event;
            getmouse(&mouse_event);
            ch = mouse_event.bstate;
        }

        switch (ch) {
            case KEY_ESCAPE:
            case 'q':
            case 'h':
                show_help = false;
                help_scroll = 0;
                break;
            case MOUSE_SCROLL_DOWN:
            case KEY_DOWN:
            case 'j':
                if (help_scroll + getmaxy(stdscr) < get_help_length()) help_scroll++;
                break;
            case MOUSE_SCROLL_UP:
            case KEY_UP:
            case 'k':
                if (help_scroll > 0) help_scroll--;
                break;
        }
    }
}

int main(int argc, char **argv) {
    if (argc > 1) {
        fprintf(stderr, "usage: %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!is_git_initialized()) ERROR("Git is not initialized in the current directory.\n");
    get_git_state(&state);

    setup_signal_handlers();
    poll_init();
    ui_init();
    atexit(cleanup);
    render(&state);

    int scroll = 0, cursor = 0, selection = -1;
    while (running) {
        clear();

        if (getmaxx(stdscr) < MIN_WIDTH || getmaxy(stdscr) < MIN_HEIGHT) {
            printw("Screen is too small! Make sure it is at least %dx%d.\n", MIN_WIDTH, MIN_HEIGHT);
            handle_info();
            continue;
        }

        if (is_state_empty(&state)) {
            printw("There are no unstaged changes.\n");
            handle_info();
            continue;
        }

        if (show_help) {
            handle_help();
            continue;
        }

        // make sure there is always content on the screen
        if (scroll > get_lines_length()) scroll = MAX(0, get_lines_length() - cursor);

        int height = getmaxy(stdscr);
        int y = scroll + cursor;
        int selection_start = -1;
        int selection_end = -1;
        if (selection != -1) {
            selection_start = MIN(selection, y);
            selection_end = MAX(selection, y);
        }

        height -= output(scroll, cursor, selection_start, selection_end);
        move(cursor, 0);
        refresh();

        if (!poll_events(&state)) continue;

        int ch;
        while ((ch = getch()) != ERR) {
            if (ch == KEY_MOUSE) {
                MEVENT mouse_event;
                getmouse(&mouse_event);
                ch = mouse_event.bstate;
            }

            switch (ch) {
                case KEY_ESCAPE:
                case 'q':
                    running = false;
                    break;
                case 'h':
                    show_help = true;
                    break;
                case MOUSE_SCROLL_DOWN:
                case KEY_DOWN:
                case 'j':
                    if (cursor < height - 1 - SCROLL_PADDING) cursor++;
                    else if (scroll + height < get_lines_length()) {
                        if (cursor > height - 1 - SCROLL_PADDING) cursor--;
                        scroll++;
                    } else if (cursor < height - 1) cursor++;

                    if (!is_selectable(scroll + cursor)) selection = -1;

                    break;
                case 'J': {
                    int hunk_y = get_next_hunk(y);
                    if (hunk_y == -1) break;

                    if (hunk_y < scroll + getmaxy(stdscr) - SCROLL_PADDING) cursor = hunk_y - scroll;
                    else scroll = hunk_y - cursor;
                } break;
                case MOUSE_SCROLL_UP:
                case KEY_UP:
                case 'k':
                    if (cursor > SCROLL_PADDING) cursor--;
                    else if (scroll > 0) scroll--;
                    else if (cursor > 0) cursor--;

                    if (!is_selectable(scroll + cursor)) selection = -1;

                    break;
                case 'K': {
                    int hunk_y = get_prev_hunk(y);
                    if (hunk_y == -1) break;

                    if (hunk_y <= SCROLL_PADDING) {
                        scroll = 0;
                        cursor = hunk_y;
                    } else if (hunk_y > scroll + SCROLL_PADDING) cursor = hunk_y - scroll;
                    else scroll = hunk_y - cursor;
                } break;
                default:
                    if (y < get_lines_length()) {
                        int result = invoke_action(y, ch, selection_start, selection_end);
                        if (result & AC_UPDATE_STATE) {
                            poll_ignore_change();
                            update_git_state(&state);
                            render(&state);
                        }
                        if (result & AC_RERENDER) render(&state);
                        if (result & AC_TOGGLE_SELECTION) selection = selection == -1 ? y : -1;
                    }
            }
        }
    }

    return EXIT_SUCCESS;
}
