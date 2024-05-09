#include "exec.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include "error.h"

#define INITIAL_BUFFER_SIZE 1024

#define NO_GIT_BINARY 100  // exit code for forked process

#define OPEN_PIPE(read_fd, write_fd)                                              \
    int read_fd, write_fd;                                                        \
    do {                                                                          \
        int fds[2];                                                               \
        if (pipe(fds) == -1) ERROR("Couldn't open pipe: %s.\n", strerror(errno)); \
        read_fd = fds[0];                                                         \
        write_fd = fds[1];                                                        \
    } while (0);

int gexec(char *const *args) {
    ASSERT(args != NULL);

    pid_t pid = fork();
    if (pid == -1) ERROR("Couldn't fork process: %s.\n", strerror(errno));
    if (pid == 0) {
        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd == -1) exit(1);
        if (dup2(null_fd, STDOUT_FILENO) == -1) exit(1);
        if (dup2(null_fd, STDERR_FILENO) == -1) exit(1);
        close(null_fd);

        if (execvp("git", args) == -1) exit(errno == ENOENT ? NO_GIT_BINARY : 1);
    }

    int exit_code;
    if (waitpid(pid, &exit_code, 0) == -1) ERROR("Couldn't wait for child process: %s.\n", strerror(errno));
    if (WEXITSTATUS(exit_code) == NO_GIT_BINARY) ERROR("Couldn't find git binary. Make sure it is in PATH.\n");

    return WEXITSTATUS(exit_code);
}

char *pipe_read(int fd) {
    size_t buffer_capacity = INITIAL_BUFFER_SIZE;
    size_t buffer_size = buffer_capacity;
    size_t buffer_offset = 0;

    char *buffer = (char *) malloc(buffer_capacity + 1);
    if (buffer == NULL) OUT_OF_MEMORY();

    int bytes;
    while ((bytes = read(fd, buffer + buffer_offset, buffer_size)) > 0 || (bytes == -1 && errno == EINTR)) {
        if (bytes == -1) continue;

        buffer_size -= bytes;
        buffer_offset += bytes;

        if (buffer_size == 0) {
            buffer_size = buffer_capacity;
            buffer_capacity *= 2;
            buffer = (char *) realloc(buffer, buffer_capacity + 1);
            if (buffer == NULL) OUT_OF_MEMORY();
        }
    }
    if (bytes == -1) {
        free(buffer);
        return NULL;
    }

    buffer[buffer_offset] = '\0';
    return buffer;
}

char *gexecr(char *const *args) {
    ASSERT(args != NULL);

    OPEN_PIPE(read_fd, write_fd);
    OPEN_PIPE(error_read_fd, error_write_fd);

    pid_t pid = fork();
    if (pid == -1) ERROR("Couldn't fork process: %s.\n", strerror(errno));
    if (pid == 0) {
        if (close(read_fd) == -1) exit(1);
        if (close(error_read_fd) == -1) exit(1);

        if (dup2(write_fd, STDOUT_FILENO) == -1) exit(1);
        if (dup2(error_write_fd, STDERR_FILENO) == -1) exit(1);

        if (execvp("git", args) == -1) exit(errno == ENOENT ? NO_GIT_BINARY : 1);
    }

    if (close(write_fd) == -1 || close(error_write_fd) == -1) ERROR("Couldn't close pipe: %s.\n", strerror(errno));

    char *buffer = pipe_read(read_fd);
    if (buffer == NULL) ERROR("Couldn't read from child's pipe.\n");

    int exit_code;
    if (waitpid(pid, &exit_code, 0) == -1) ERROR("Couldn't wait for child process: %s.\n", strerror(errno));
    if (WEXITSTATUS(exit_code) == NO_GIT_BINARY) ERROR("Couldn't find git binary. Make sure it is in PATH.\n");
    if (WEXITSTATUS(exit_code) != 0) {
        char *buffer = pipe_read(error_read_fd);
        ERROR("Childs process exited with non-zero exit code. Child's stderr:\n%s\n", buffer);
    }

    if (close(read_fd) == -1 || close(error_read_fd) == -1) ERROR("Couldn't close pipe: %s.\n", strerror(errno));
    return buffer;
}

int gexecw(char *const *args, const char *buffer) {
    ASSERT(args != NULL && buffer != NULL);

    OPEN_PIPE(read_fd, write_fd);

    pid_t pid = fork();
    if (pid == -1) ERROR("Couldn't fork process: %s.\n", strerror(errno));
    if (pid == 0) {
        if (close(write_fd) == -1) exit(1);

        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd == -1) exit(1);
        if (dup2(read_fd, STDIN_FILENO) == -1) exit(1);
        if (dup2(null_fd, STDOUT_FILENO) == -1) exit(1);
        if (dup2(null_fd, STDERR_FILENO) == -1) exit(1);
        close(null_fd);

        if (execvp("git", args) == -1) exit(1);
    }

    if (close(read_fd) == -1) ERROR("Couldn't close pipe: %s.\n", strerror(errno));

    ssize_t patch_size = strlen(buffer);
    if (write(write_fd, buffer, patch_size) != patch_size) ERROR("Couldn't write the entire buffer.\n");
    if (close(write_fd) == -1) ERROR("Couldn't close pipe: %s.\n", strerror(errno));

    int exit_code;
    if (waitpid(pid, &exit_code, 0) == -1) ERROR("Couldn't wait for child process: %s.\n", strerror(errno));
    return exit_code;
}
