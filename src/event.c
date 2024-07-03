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

static struct pollfd poll_fds[2];
static int events_fd = -1;
static bool ignore_event = false;

#ifdef __linux__

#include <sys/inotify.h>

#define EVENT_MASK (IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE | IN_DELETE_SELF | IN_MOVE_SELF)

#define MAX_PATH_LENGTH 4096

// Recursively adds directories to inotify
// NOTE: modifies path, which must fit longest possible path.
static void watch_dir(char *path) {
    ASSERT(path != NULL);
    if (is_ignored(path)) return;

    if (inotify_add_watch(events_fd, path, EVENT_MASK) == -1) ERROR("Unable to watch directory \"%s\": %s.\n", path, strerror(errno));

    DIR *dir = opendir(path);
    if (dir == NULL) ERROR("Unable to open directory \"%s\": %s.\n", path, strerror(errno));

    size_t path_len = strlen(path);
    path[path_len++] = '/';

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR) continue;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".git") == 0) continue;

        size_t dir_len = strlen(entry->d_name);
        ASSERT(path_len + dir_len + 1 <= MAX_PATH_LENGTH);
        memcpy(path + path_len, entry->d_name, dir_len);
        path[path_len + dir_len] = '\0';

        watch_dir(path);
    }

    closedir(dir);
}

static void watch_dirs(void) {
    char path_buffer[MAX_PATH_LENGTH] = ".";
    watch_dir(path_buffer);
    if (inotify_add_watch(events_fd, ".git", EVENT_MASK) == -1) ERROR("Unable to watch \".git\": %s.\n", strerror(errno));
}

#else

#include <CoreServices/CoreServices.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

static pid_t watch_thread_pid = -1;
static int event_write_fd = -1;
static FSEventStreamRef stream = NULL;

static void fs_event_handler(ConstFSEventStreamRef stream, void *clientInfo, size_t numEvents, void *paths,
                             const FSEventStreamEventFlags flags[], const FSEventStreamEventId ids[]) {
    (void) stream;
    (void) clientInfo;
    (void) paths;
    (void) flags;
    (void) ids;

    ASSERT(event_write_fd != -1);
    if (numEvents == 0) return;

    char event = 1;
    if (write(event_write_fd, &event, 1) != 1) ERROR("Unable to write: %s.\n", strerror(errno));
}

static void signal_handler(int signal) {
    (void) signal;

    ASSERT(stream != NULL);
    FSEventStreamStop(stream);
    FSEventStreamSetDispatchQueue(stream, NULL);
    FSEventStreamInvalidate(stream);
    FSEventStreamRelease(stream);
    exit(0);
}

static void macos_watch_thread(void) {
    CFStringRef path = CFSTR(".");
    CFArrayRef paths = CFArrayCreate(NULL, (const void **) &path, 1, NULL);

    stream = FSEventStreamCreate(NULL, &fs_event_handler, NULL, paths, kFSEventStreamEventIdSinceNow, 1.0, 0);

    CFRelease(path);
    CFRelease(paths);

    FSEventStreamSetDispatchQueue(stream, dispatch_get_main_queue());
    FSEventStreamStart(stream);

    struct sigaction action = {0};
    action.sa_handler = &signal_handler;
    action.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &action, NULL);

    dispatch_main();
}

#endif

void poll_init(void) {
    char *root_path = get_git_root_path();
    if (chdir(root_path) == -1) ERROR("Unable to cd into \"%s\": %s.\n", root_path, strerror(errno));
    free(root_path);

#ifdef __linux__
    events_fd = inotify_init1(IN_NONBLOCK);
    if (events_fd == -1) ERROR("Unable to initialize inotify: %s.\n", strerror(errno));

    watch_dirs();
#else
    int fds[2];
    if (pipe(fds) == -1) ERROR("Unable to create pipe: %s.\n", strerror(errno));

    watch_thread_pid = fork();
    if (watch_thread_pid == -1) ERROR("Unable to create a watching thread: %s\n", strerror(errno));
    if (watch_thread_pid == 0) {
        event_write_fd = fds[1];
        close(fds[0]);
        macos_watch_thread();
        exit(0);
    }

    events_fd = fds[0];
    int flags = fcntl(events_fd, F_GETFL, 0);
    if (flags == -1 || fcntl(events_fd, F_SETFL, flags | O_NONBLOCK) == -1)
        ERROR("Unable to make file description non-blocking: %s.\n", strerror(errno));

    close(fds[1]);
#endif

    poll_fds[0] = (struct pollfd){STDIN_FILENO, POLLIN, 0};
    poll_fds[1] = (struct pollfd){events_fd, POLLIN, 0};
}

void poll_cleanup(void) {
#ifndef __linux__
    if (watch_thread_pid != -1) {
        kill(watch_thread_pid, SIGINT);
        waitpid(watch_thread_pid, NULL, 0);
    }
#endif

    if (events_fd != -1) close(events_fd);
}

bool poll_events(State *state) {
    static char event_buffer[1024];

    if (poll(poll_fds, sizeof(poll_fds) / sizeof(poll_fds[0]), -1) == -1) {
        if (errno == EINTR) return false;
        ERROR("Unable to poll: %s.\n", strerror(errno));
    }

    if ((poll_fds[1].revents & POLLIN) == 0) return true;

#ifdef __linux__
    bool reindex = false;

    ssize_t bytes;
    while ((bytes = read(events_fd, event_buffer, sizeof(event_buffer))) > 0) {
        if (reindex) continue;

        for (ssize_t i = 0; i < bytes; i += sizeof(struct inotify_event)) {
            struct inotify_event *event = (struct inotify_event *) (event_buffer + i);
            i += event->len;

            if ((event->mask & IN_CREATE) == IN_CREATE) {
                reindex = true;
                break;
            }
        }
    }
    if (bytes == -1 && errno != EAGAIN) ERROR("Unable to read inotify event: %s\n", strerror(errno));

    if (reindex) watch_dirs();
#else
    ssize_t bytes;
    while ((bytes = read(events_fd, event_buffer, sizeof(event_buffer))) > 0) continue;
    if (bytes == -1 && errno != EAGAIN) ERROR("Unable to read from pipe: %s.\n", strerror(errno));
#endif

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
