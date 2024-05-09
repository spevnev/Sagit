#define _DEFAULT_SOURCE
#include "event.h"
#include <dirent.h>
#include <errno.h>
#include <ncurses.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include "error.h"
#include "git/git.h"
#include "git/state.h"
#include "ui/ui.h"
#include "vector.h"

#ifdef __linux__

#include <sys/inotify.h>

#define NEW_FOLDER (IN_CREATE | IN_ISDIR)
#define EVENT_MASK (IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM)

#else

#include <fcntl.h>
#include <sys/event.h>

#define NEW_FOLDER (NOTE_WRITE | NOTE_LINK)
#define EVENT_MASK (NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_RENAME)

static int_vec kqueue_fds = {0};
static int git_index_fd = -1;

#endif

#define MAX_PATH_LENGTH 4096

static char path_buffer[MAX_PATH_LENGTH];
static struct pollfd poll_fds[2];
static int events_fd = -1;
static bool ignore_event = false;

static void watch_git_index(void) {
#ifdef __linux__
    static const char *path = ".git";

    if (inotify_add_watch(events_fd, path, EVENT_MASK) == -1) ERROR("Unable to watch \"%s\": %s.\n", path, strerror(errno));
#else
    static const char *path = ".git/index";

    if (git_index_fd != -1) close(git_index_fd);
    git_index_fd = open(path, O_RDONLY);

    static struct kevent event;
    EV_SET(&event, git_index_fd, EVFILT_VNODE, EV_ADD | EV_CLEAR, NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND, 0, 0);
    if (kevent(events_fd, &event, 1, NULL, 0, NULL) == -1) ERROR("Unable to watch \"%s\": %s.\n", path, strerror(errno));
#endif
}

// Recursively adds directories to inotify(Linux) or kqueue(BSD/MacOS).
// NOTE: modifies path, which must fit longest possible path.
static void watch_dir(char *path) {
    ASSERT(path != NULL);
    if (is_ignored(path)) return;

    int status;
#ifdef __linux__
    status = inotify_add_watch(events_fd, path, EVENT_MASK);
#else
    static struct kevent event;
    int fd = open(path, O_RDONLY | O_DIRECTORY);
    EV_SET(&event, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR, EVENT_MASK, 0, 0);
    status = kevent(events_fd, &event, 1, NULL, 0, NULL);
    VECTOR_PUSH(&kqueue_fds, fd);
#endif
    if (status == -1) ERROR("Unable to watch directory \"%s\": %s.\n", path, strerror(errno));

    DIR *dir = opendir(path);
    if (dir == NULL) ERROR("Unable to open directory \"%s\": %s.\n", path, strerror(errno));

    size_t path_len = strlen(path);
    path[path_len++] = '/';

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
#ifdef __linux__
        if (entry->d_type != DT_DIR) continue;
#endif

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".git") == 0) continue;

        size_t dir_len = strlen(entry->d_name);
        ASSERT(path_len + dir_len + 1 <= MAX_PATH_LENGTH);
        memcpy(path + path_len, entry->d_name, dir_len);
        path[path_len + dir_len] = '\0';

#ifdef __linux__
        watch_dir(path);
#else
        if (entry->d_type == DT_DIR) {
            watch_dir(path);
        } else if (entry->d_type == DT_REG || entry->d_type == DT_LNK) {
            int fd = open(path, O_RDONLY);
            EV_SET(&event, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR, NOTE_WRITE | NOTE_EXTEND, 0, 0);
            if (kevent(events_fd, &event, 1, NULL, 0, NULL) == -1) ERROR("Unable to add file \"%s\" to kqueue: %s.\n", path, strerror(errno));
            VECTOR_PUSH(&kqueue_fds, fd);
        }
#endif
    }

    closedir(dir);
}

static void watch_dirs(void) {
#ifndef __linux__
    for (size_t i = 0; i < kqueue_fds.length; i++) close(kqueue_fds.data[i]);
    VECTOR_RESET(&kqueue_fds);
#endif

    path_buffer[0] = '.';
    path_buffer[1] = '\0';
    watch_dir(path_buffer);
    watch_git_index();
}

void poll_init(void) {
#ifdef __linux__
    events_fd = inotify_init1(IN_NONBLOCK);
    if (events_fd == -1) ERROR("Unable to initialize inotify: %s.\n", strerror(errno));
#else
    events_fd = kqueue();
    if (events_fd == -1) ERROR("Unable to create kqueue: %s.\n", strerror(errno));
#endif
    watch_dirs();

    poll_fds[0] = (struct pollfd){STDIN_FILENO, POLLIN, 0};
    poll_fds[1] = (struct pollfd){events_fd, POLLIN, 0};
}

void poll_cleanup(void) {
#ifndef __linux__
    for (size_t i = 0; i < kqueue_fds.length; i++) close(kqueue_fds.data[i]);
    VECTOR_FREE(&kqueue_fds);
#endif

    close(events_fd);
}

bool poll_events(State *state) {
    if (poll(poll_fds, sizeof(poll_fds) / sizeof(poll_fds[0]), -1) == -1) {
        if (errno == EINTR) return false;
        ERROR("Unable to poll: %s.\n", strerror(errno));
    }

    if ((poll_fds[1].revents & POLLIN) == 0) return true;

    bool reindex = false;

#ifdef __linux__
    static char event_buffer[1024];

    ssize_t bytes;
    while ((bytes = read(events_fd, event_buffer, sizeof(event_buffer))) > 0) {
        if (reindex) continue;

        char *ptr = event_buffer;
        for (int i = 0; i < bytes; i += sizeof(struct inotify_event)) {
            struct inotify_event *event = (struct inotify_event *) (ptr + i);
            i += event->len;

            if ((event->mask & NEW_FOLDER) == NEW_FOLDER) {
                reindex = true;
                break;
            }
        }
    }
    if (bytes == -1 && errno != EAGAIN) ERROR("Unable to read inotify event.\n");
#else
    static struct kevent event;
    static struct timespec time = {0};

    int events;
    while ((events = kevent(events_fd, NULL, 0, &event, 1, &time)) > 0) {
        if ((event.fflags & NEW_FOLDER) == NEW_FOLDER) reindex = true;
        if ((int) event.ident == git_index_fd && event.fflags == NOTE_DELETE) watch_git_index();
    }
    if (events == -1) ERROR("Unable to read kevent: %s.\n", strerror(errno));
#endif

    if (reindex) watch_dirs();

    if (ignore_event) {
        ignore_event = false;
    } else {
        // if a key is pressed during external update, ignore that key
        if (poll_fds[0].revents & POLLIN) {
            while (getch() != ERR) continue;
        }

        update_git_state(state);
        render(state);

        return false;
    }

    return true;
}

void poll_ignore_event(void) { ignore_event = true; }
