#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "utils/error.h"

#define INITIAL_BUFFER_SIZE 1024

#define NO_GIT_BINARY 100  // exit code for forked process

int gexec(char *const *args) {
    ASSERT(args != NULL);

    pid_t pid = fork();
    if (pid == -1) ERROR("Couldn't fork process.\n");
    if (pid == 0) {
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        if (execvp("git", args) == -1) exit(errno == ENOENT ? NO_GIT_BINARY : 1);
    }

    int exit_code;
    if (waitpid(pid, &exit_code, 0) == -1) ERROR("Couldn't wait for child process.\n");
    if (WEXITSTATUS(exit_code) == NO_GIT_BINARY) ERROR("Couldn't find git binary. Make sure it is in PATH.\n");
    return exit_code;
}

char *pipe_read(int fd) {
    size_t buffer_capacity = INITIAL_BUFFER_SIZE;
    size_t buffer_size = buffer_capacity;
    size_t buffer_offset = 0;

    char *buffer = (char *) malloc(buffer_capacity + 1);
    if (buffer == NULL) ERROR("Process is out of memory.\n");

    int bytes;
    while ((bytes = read(fd, buffer + buffer_offset, buffer_size)) > 0 || (bytes == -1 && errno == EINTR)) {
        if (bytes == -1) continue;

        buffer_size -= bytes;
        buffer_offset += bytes;

        if (buffer_size == 0) {
            buffer_size = buffer_capacity;
            buffer_capacity *= 2;
            buffer = (char *) realloc(buffer, buffer_capacity + 1);
            if (buffer == NULL) ERROR("Process is out of memory.\n");
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

    int pipe_fds[2];
    int error_pipe_fds[2];
    if (pipe(pipe_fds) == -1) ERROR("Couldn't open pipe.\n");
    if (pipe(error_pipe_fds) == -1) ERROR("Couldn't open pipe.\n");
    int read_fd = pipe_fds[0];
    int write_fd = pipe_fds[1];
    int error_read_fd = error_pipe_fds[0];
    int error_write_fd = error_pipe_fds[1];

    pid_t pid = fork();
    if (pid == -1) ERROR("Couldn't fork process.\n");
    if (pid == 0) {
        if (close(read_fd) == -1) exit(1);
        if (close(error_read_fd) == -1) exit(1);

        if (dup2(write_fd, STDOUT_FILENO) == -1) exit(1);
        if (dup2(error_write_fd, STDERR_FILENO) == -1) exit(1);

        if (execvp("git", args) == -1) exit(errno == ENOENT ? NO_GIT_BINARY : 1);
    }

    if (close(write_fd) == -1) ERROR("Couldn't close pipe.\n");
    if (close(error_write_fd) == -1) ERROR("Couldn't close pipe.\n");

    char *buffer = pipe_read(read_fd);
    if (buffer == NULL) ERROR("Couldn't read from child's pipe.\n");

    int exit_code;
    if (waitpid(pid, &exit_code, 0) == -1) ERROR("Couldn't wait for child process.\n");
    if (WEXITSTATUS(exit_code) == NO_GIT_BINARY) ERROR("Couldn't find git binary. Make sure it is in PATH.\n");
    if (WEXITSTATUS(exit_code) != 0) {
        char *buffer = pipe_read(error_read_fd);
        ERROR("Childs process exited with non-zero exit code. Child's stderr:\n%s\n", buffer);
    }

    if (close(error_read_fd) == -1) ERROR("Couldn't close pipe.\n");
    return buffer;
}

int gexecw(char *const *args, const char *buffer) {
    ASSERT(args != NULL && buffer != NULL);

    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) ERROR("Couldn't open pipe.\n");
    int read_fd = pipe_fds[0];
    int write_fd = pipe_fds[1];

    pid_t pid = fork();
    if (pid == -1) ERROR("Couldn't fork process.\n");
    if (pid == 0) {
        if (close(write_fd) == -1) exit(1);

        if (dup2(read_fd, STDIN_FILENO) == -1) exit(1);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        if (execvp("git", args) == -1) exit(1);
    }

    if (close(read_fd) == -1) ERROR("Couldn't close pipe.\n");

    ssize_t patch_size = strlen(buffer);
    ssize_t bytes = write(write_fd, buffer, patch_size);
    if (bytes != patch_size) ERROR("Couldn't write the entire buffer.\n");
    if (close(write_fd) == -1) ERROR("Couldn't close pipe.\n");

    int exit_code;
    if (waitpid(pid, &exit_code, 0) == -1) ERROR("Couldn't wait for child process.\n");
    return exit_code;
}