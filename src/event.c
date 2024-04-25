#define _DEFAULT_SOURCE

#include "event.h"
#include <dirent.h>
#include <errno.h>
#include <ncurses.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include "git/git.h"
#include "git/state.h"
#include "ui/ui.h"
#include "utils/error.h"

#define MAX_PATH_LENGTH 4096

#ifdef __linux__
    #include <sys/inotify.h>
static int inotify_fd = -1;
static bool ignore_inotify = false;
#endif

#ifdef __linux__
static struct pollfd poll_fds[2];
#else
static struct pollfd poll_fds[1];
#endif

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
        ASSERT(path_len + name_length + 1 <= MAX_PATH_LENGTH);
        memcpy(path + path_len + 1, entry->d_name, name_length);
        path[path_len + 1 + name_length] = '\0';

        watch_dir(path);
    }

    closedir(dir);
}
#endif

void poll_init(void) {
    poll_fds[0] = (struct pollfd){STDIN_FILENO, POLLIN, 0};

#ifdef __linux__
    inotify_fd = inotify_init1(IN_NONBLOCK);
    poll_fds[1] = (struct pollfd){inotify_fd, POLLIN, 0};

    char path_buffer[MAX_PATH_LENGTH] = ".";
    watch_dir(path_buffer);
#endif
}

void poll_cleanup(void) {
#ifdef __linux__
    close(inotify_fd);
#endif
}

bool poll_events(State *state) {
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

            update_git_state(state);
            render(state, NULL);
            return false;
        }
    }
#endif

    return true;
}

void poll_ignore_change(void) {
#ifdef __linux__
    ignore_inotify = true;
#endif
}
