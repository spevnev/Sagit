#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include <dirent.h>
#include <ncurses.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
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

#define SCREEN_PADDING 2

static int running = 1;
static State state = {0};

static void cleanup(void) {
    ui_cleanup();
    free_state(&state);
}

static void stop_running(int signal) {
    ASSERT(signal == SIGINT);
    running = 0;
}

#ifdef __linux__
static void resize(int signal) {
    ASSERT(signal == SIGWINCH);

    struct winsize win;
    ioctl(0, TIOCGWINSZ, &win);
    resizeterm(win.ws_row, win.ws_col);
}

// Recursively adds directory and its subdirectories to inotify.
// NOTE: modifies path, which must fit longest possible path.
static void watch_dirs_rec(int inotify_fd, char *path) {
    if (is_ignored(path)) return;

    if (inotify_add_watch(inotify_fd, path, IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO) == -1)
        ERROR("Unable to watch a directory \"%s\".\n", path);

    DIR *dir = opendir(path);
    ASSERT(dir != NULL);

    size_t path_len = strlen(path);
    path[path_len] = '/';

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        size_t name_length = strlen(entry->d_name);
        memcpy(path + path_len + 1, entry->d_name, name_length);
        path[path_len + 1 + name_length] = '\0';

        watch_dirs_rec(inotify_fd, path);
    }

    closedir(dir);
}
#endif

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

#ifdef __linux__
    action.sa_handler = resize;
    sigaction(SIGWINCH, &action, NULL);

    char event_buffer[1024];
    char path_buffer[4096] = ".";

    int inotify_fd = inotify_init1(IN_NONBLOCK);
    watch_dirs_rec(inotify_fd, path_buffer);

    struct pollfd poll_fds[2] = {{STDIN_FILENO, POLLIN, 0}, {inotify_fd, POLLIN, 0}};
#endif

    ui_init();
    atexit(cleanup);
    render(&state);

    int show_help = 0;
    int saved_scroll = 0;
    int scroll = 0;
    int cursor = 0;
    int selection = -1;
    int ignore_inotify = 0;
    MEVENT mouse_event = {0};
    while (running) {
        if (getmaxx(stdscr) < MIN_WIDTH || getmaxy(stdscr) < MIN_HEIGHT) {
            clear();
            printw("Screen is too small! Make sure it is at least %dx%d.\n", MIN_WIDTH, MIN_HEIGHT);
            refresh();

#ifdef __linux__
            nodelay(stdscr, false);
            if (getch() == 'q') running = 0;
            nodelay(stdscr, true);
#else
            if (getch() == 'q') running = 0;
#endif
            continue;
        }

        if (is_state_empty(&state)) {
            clear();
            printw("There are no unstaged changes.\n");
            refresh();

#ifdef __linux__
            int events = poll(poll_fds, 2, -1);
            if (events == -1) {
                if (errno == EINTR) continue;
                ERROR("Unable to poll.\n");
            }
            ASSERT(events > 0);

            if (poll_fds[1].revents & POLLIN) {
                char buffer[1024];
                ssize_t bytes;
                int new_dir = 0;
                while ((bytes = read(inotify_fd, buffer, sizeof(buffer))) > 0) {
                    struct inotify_event *event = (struct inotify_event *) buffer;
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
                    watch_dirs_rec(inotify_fd, path_buffer);
                }

                update_git_state(&state);
                render(&state);
                continue;
            }

            if ((poll_fds[0].revents & POLLIN) == 0) continue;
            if (getch() == 'q') running = 0;
#else
            nodelay(stdscr, false);
            if (getch() == 'q') running = 0;
            nodelay(stdscr, true);
#endif
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
            output(scroll, cursor, selection_start, selection_end);
            move(cursor, 0);
        }
        refresh();

#ifdef __linux__
        int events = poll(poll_fds, 2, -1);
        if (events == -1) {
            if (errno == EINTR) continue;
            ERROR("Unable to poll.\n");
        }

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
                watch_dirs_rec(inotify_fd, path_buffer);
            }

            if (ignore_inotify) {
                ignore_inotify = 0;
            } else {
                // if a key is pressed during external update, ignore that key
                if (poll_fds[0].revents & POLLIN) {
                    while (getch() != ERR) continue;
                }

                update_git_state(&state);
                render(&state);
                continue;
            }
        }

        if ((poll_fds[0].revents & POLLIN) == 0) continue;
#else
        (void) ignore_inotify;
#endif

        int ch;
        while ((ch = getch()) != ERR) {
            int mouse = 0;
            if (ch == KEY_MOUSE) {
                getmouse(&mouse_event);
                mouse = mouse_event.bstate;
            }

            if (show_help) {
                if (ch == 'q' || ch == KEY_ESCAPE || ch == 'h') {
                    show_help = 0;
                    scroll = saved_scroll;
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
                } else if (ch == 'j' || ch == KEY_DOWN || mouse == MOUSE_SCROLL_DOWN) {
                    if (cursor < height - 1 - SCREEN_PADDING) cursor++;
                    else if (scroll + height < get_lines_length()) scroll++;
                    else if (cursor < height - 1) cursor++;

                    if (!is_selectable(scroll + cursor)) selection = -1;
                } else if (ch == 'k' || ch == KEY_UP || mouse == MOUSE_SCROLL_UP) {
                    if (cursor > SCREEN_PADDING) cursor--;
                    else if (scroll > 0) scroll--;
                    else if (cursor > 0) cursor--;

                    if (!is_selectable(scroll + cursor)) selection = -1;
                } else if (y < get_lines_length()) {
                    int result = invoke_action(y, ch, selection_start, selection_end);
                    if (result & AC_UPDATE_STATE) {
                        ignore_inotify = 1;
                        update_git_state(&state);
                    }
                    if ((result & AC_RERENDER) || (result & AC_UPDATE_STATE)) render(&state);
                    if (result & AC_TOGGLE_SELECTION) {
                        if (selection == -1) selection = y;
                        else selection = -1;
                    }
                }
            }
        }
    }

#ifdef __linux__
    close(inotify_fd);
#endif

    return EXIT_SUCCESS;
}
