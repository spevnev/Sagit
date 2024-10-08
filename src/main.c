#include <ncurses.h>
#include <stdio.h>
#include "config.h"
#include "error.h"
#include "event.h"
#include "git/git.h"
#include "git/state.h"
#include "signals.h"
#include "ui/action.h"
#include "ui/help.h"
#include "ui/ui.h"
#include "vector.h"

#if !defined(__linux__) && !defined(__APPLE__)
#warning WARNING: Sagit only supports Linux and MacOS
#endif

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

static void handle_info(void) {
    refresh();

    if (!poll_events(&state)) return;

    int ch;
    while ((ch = getch()) != ERR) {
        if (ch == 'q' || ch == KEY_ESCAPE) running = 0;
    }
}

static void handle_help(void) {
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

static void scroll_up_to(int y, int *scroll, int *cursor) {
    if (y > *scroll + SCROLL_PADDING) {
        *cursor = y - *scroll;
    } else if (y < *cursor) {
        *scroll = 0;
        *cursor = y;
    } else {
        *scroll = y - *cursor;
    }
}

static void handle_main(void) {
    static int scroll = 0, cursor = 0, selection = -1;

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
                running = false;
                break;
            case 'h':
                show_help = true;
                break;
            case 'r':
                update_git_state(&state);
                render(&state);
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

                selection = -1;
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

                selection = -1;
                scroll_up_to(hunk_y, &scroll, &cursor);
            } break;
            default:
                if (y < get_lines_length()) {
                    int result = invoke_action(y, ch, selection_start, selection_end);
                    if (result & AC_UPDATE_STATE) {
                        poll_ignore_event();
                        update_git_state(&state);
                        if (cursor != selection_start && selection_start != -1) scroll_up_to(selection_start, &scroll, &cursor);
                        render(&state);
                    }
                    if (result & AC_RERENDER) render(&state);
                    if (result & AC_TOGGLE_SELECTION) selection = selection == -1 ? y : -1;
                }
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

    poll_init();
    ui_init();
    setup_signal_handlers();
    atexit(cleanup);
    render(&state);

    while (running) {
        clear();

        if (getmaxx(stdscr) < MIN_WIDTH || getmaxy(stdscr) < MIN_HEIGHT) {
            printw("Screen is too small! Make sure it is at least %dx%d.\n", MIN_WIDTH, MIN_HEIGHT);
            handle_info();
            continue;
        }

        if (is_state_empty(&state)) {
            printw("There are no uncommitted changes.\n");
            handle_info();
            continue;
        }

        if (show_help) {
            handle_help();
            continue;
        }

        handle_main();
    }

    return EXIT_SUCCESS;
}
