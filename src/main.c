#if __APPLE__
    #define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include <dirent.h>
#include <errno.h>
#include <ncurses.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "config.h"
#include "git/git.h"
#include "git/state.h"
#include "ui/action.h"
#include "ui/help.h"
#include "ui/ui.h"
#include "utils/error.h"
#ifdef __linux__
    #include <sys/inotify.h>
#endif

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))

#define MIN_WIDTH 80
#define MIN_HEIGHT 10

static bool running = true;
static bool show_help = false;
static State state = {0};
static size_vec hunk_idxs = {0};

#ifdef __linux__
static int inotify_fd = -1;
static bool ignore_inotify = false;

static struct pollfd poll_fds[2];
#else
static struct pollfd poll_fds[1];
#endif

static void cleanup(void) {
#ifdef __linux__
    close(inotify_fd);
#endif
    ui_cleanup();
    free_state(&state);
    VECTOR_FREE(&hunk_idxs);
}

static void stop_running(int signal) {
    ASSERT(signal == SIGINT);
    running = false;
}

static void resize(int signal) {
    ASSERT(signal == SIGWINCH);

    struct winsize win;
    ioctl(0, TIOCGWINSZ, &win);
    if (resizeterm(win.ws_row, win.ws_col) == ERR) ERROR("Unable to resize terminal window: %s.\n", strerror(errno));
}

#ifdef __linux__
// Recursively adds directory and its subdirectories to inotify.
// NOTE: modifies path, which must fit longest possible path.
static void watch_dir(char *path) {
    ASSERT(path != NULL);
    if (is_ignored(path)) return;

    if (inotify_add_watch(inotify_fd, path, IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO) == -1)
        ERROR("Unable to watch a directory \"%s\": %s.\n", path, strerror(errno));

    DIR *dir = opendir(path);
    if (dir == NULL) ERROR("Unable to open a directory \"%s\": %s.\n", path, strerror(errno));

    size_t path_len = strlen(path);
    path[path_len] = '/';

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        size_t name_length = strlen(entry->d_name);
        memcpy(path + path_len + 1, entry->d_name, name_length);
        path[path_len + 1 + name_length] = '\0';

        watch_dir(path);
    }

    closedir(dir);
}
#endif

bool poll_events(void) {
    int events = poll(poll_fds, sizeof(poll_fds) / sizeof(poll_fds[0]), -1);
    if (events == -1) {
        if (errno == EINTR) return false;
        ERROR("Unable to poll: %s.\n", strerror(errno));
    }

#ifdef __linux__
    static char path_buffer[4096];
    static char event_buffer[1024];

    if (poll_fds[1].revents & POLLIN) {
        ssize_t bytes;
        int new_dir = 0;
        while ((bytes = read(inotify_fd, event_buffer, sizeof(event_buffer))) > 0) {
            struct inotify_event *event = (struct inotify_event *) event_buffer;
            if (new_dir) break;
            for (int i = events; i >= 0; i--) {
                if ((event->mask & IN_CREATE) && (event->mask & IN_ISDIR)) {
                    new_dir = 1;
                    break;
                }
                event++;
            }
            continue;
        }
        if (bytes == -1 && errno != EAGAIN) ERROR("Unable to read inotify event.\n");
        if (new_dir) {
            path_buffer[0] = '.';
            path_buffer[1] = '\0';
            watch_dir(path_buffer);
        }

        if (ignore_inotify) {
            ignore_inotify = false;
        } else {
            // if a key is pressed during external update, ignore that key
            if (poll_fds[0].revents & POLLIN) {
                while (getch() != ERR) continue;
            }

            update_git_state(&state);
            render(&state, &hunk_idxs);
            return false;
        }
    }
#endif

    return true;
}

// TODO: rename
void handle_info(void) {
    refresh();

    if (!poll_events()) return;

    int ch;
    while ((ch = getch()) != ERR) {
        if (ch == 'q' || ch == KEY_ESCAPE) running = 0;
    }
}

void handle_help(void) {
    static int help_scroll = 0;

    output_help(help_scroll);
    refresh();

    if (!poll_events()) return;

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

    struct sigaction action = {0};

    action.sa_handler = stop_running;
    sigaction(SIGINT, &action, NULL);

    action.sa_handler = resize;
    sigaction(SIGWINCH, &action, NULL);

    poll_fds[0] = (struct pollfd){STDIN_FILENO, POLLIN, 0};
#ifdef __linux__
    inotify_fd = inotify_init1(IN_NONBLOCK);
    poll_fds[1] = (struct pollfd){inotify_fd, POLLIN, 0};

    char path_buffer[4096] = ".";
    watch_dir(path_buffer);
#endif

    ui_init();
    atexit(cleanup);
    render(&state, &hunk_idxs);

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

        if (!poll_events()) continue;

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
                case 'J':
                    for (size_t i = 0; i < hunk_idxs.length; i++) {
                        if (hunk_idxs.data[i] <= y) continue;

                        if (hunk_idxs.data[i] < scroll + getmaxy(stdscr) - SCROLL_PADDING) cursor = hunk_idxs.data[i] - scroll;
                        else scroll = hunk_idxs.data[i] - cursor;

                        break;
                    }
                    break;
                case MOUSE_SCROLL_UP:
                case KEY_UP:
                case 'k':
                    if (cursor > SCROLL_PADDING) cursor--;
                    else if (scroll > 0) scroll--;
                    else if (cursor > 0) cursor--;

                    if (!is_selectable(scroll + cursor)) selection = -1;

                    break;
                case 'K': {
                    int last_index = -1;
                    for (size_t i = 0; i < hunk_idxs.length; i++) {
                        if (hunk_idxs.data[i] >= y) break;
                        last_index = hunk_idxs.data[i];
                    }
                    if (last_index == -1) break;

                    if (last_index > scroll + SCROLL_PADDING) cursor = last_index - scroll;
                    else scroll = last_index - cursor;
                } break;
                default:
                    if (y < get_lines_length()) {
                        int result = invoke_action(y, ch, selection_start, selection_end);
                        if (result & AC_UPDATE_STATE) {
#ifdef __linux__
                            ignore_inotify = true;
#endif
                            update_git_state(&state);
                            render(&state, &hunk_idxs);
                        }
                        if (result & AC_RERENDER) render(&state, &hunk_idxs);
                        if (result & AC_TOGGLE_SELECTION) selection = selection == -1 ? y : -1;
                    }
            }
        }
    }

    return EXIT_SUCCESS;
}
