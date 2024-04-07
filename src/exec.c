#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "error.h"
#include "vector.h"

#define INITIAL_BUFFER_SIZE 1024

#define NO_GIT_BINARY 100  // exit code for forked process

char *git_exec(int *out_status, char *const *args) {
    assert(args != NULL);

    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) ERROR("Couldn't open pipe.\n");

    pid_t pid = fork();
    if (pid == -1) ERROR("Couldn't fork process.\n");
    if (pid == 0) {
        int write_fd = pipe_fds[1];
        if (close(pipe_fds[0]) == -1) exit(1);

        if (dup2(write_fd, STDOUT_FILENO) == -1) exit(1);
        if (dup2(write_fd, STDERR_FILENO) == -1) exit(1);

        if (execvp("git", args) == -1) exit(errno == ENOENT ? NO_GIT_BINARY : 1);
    }

    int read_fd = pipe_fds[0];
    if (close(pipe_fds[1]) == -1) ERROR("Couldn't close pipe.\n");

    size_t buffer_capacity = INITIAL_BUFFER_SIZE;
    size_t buffer_size = buffer_capacity;
    size_t buffer_offset = 0;
    char *buffer = (char *) malloc(buffer_capacity + 1);
    if (buffer == NULL) ERROR("Process is out of memory.\n");

    int bytes;
    while ((bytes = read(read_fd, buffer + buffer_offset, buffer_size)) > 0) {
        buffer_size -= bytes;
        buffer_offset += bytes;

        if (buffer_size == 0) {
            buffer_size = buffer_capacity;
            buffer_capacity *= 2;
            buffer = (char *) realloc(buffer, buffer_capacity + 1);
            if (buffer == NULL) ERROR("Process is out of memory.\n");
        }
    }
    if (bytes == -1) ERROR("Couldn't read from child's pipe.\n");
    buffer[buffer_offset] = '\0';

    int _status;
    int *status = out_status;
    if (out_status == NULL) status = &_status;

    if (waitpid(pid, status, 0) == -1) ERROR("Couldn't wait for child process.\n");
    if (close(read_fd) == -1) ERROR("Couldn't close pipe.\n");

    if (WEXITSTATUS(*status) == NO_GIT_BINARY) ERROR("Couldn't find git binary. Make sure it is in PATH.\n");
    if (out_status == NULL && WEXITSTATUS(_status) != 0) ERROR("Childs process exited with non-zero exit code.\n");

    return buffer;
}

int git_apply(char *const *args, const char *patch) {
    assert(args != NULL && patch != NULL);

    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) ERROR("Couldn't open pipe.\n");

    pid_t pid = fork();
    if (pid == -1) ERROR("Couldn't fork process.\n");
    if (pid == 0) {
        int read_fd = pipe_fds[0];
        if (close(pipe_fds[1]) == -1) exit(1);

        if (dup2(read_fd, STDIN_FILENO) == -1) exit(1);
        if (execvp("git", args) == -1) exit(1);
    }

    int write_fd = pipe_fds[1];
    if (close(pipe_fds[0]) == -1) ERROR("Couldn't close pipe.\n");

    ssize_t bytes = write(write_fd, patch, strlen(patch));
    if (bytes != ((ssize_t) strlen(patch))) ERROR("Couldn't write the entire patch.\n");
    if (close(write_fd) == -1) ERROR("Couldn't close pipe.\n");

    int status;
    if (waitpid(pid, &status, 0) == -1) ERROR("Couldn't wait for child process.\n");

    return status;
}

int git_apply_array(char *const *args, const str_vec *patches) {
    assert(args != NULL && patches != NULL);

    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) ERROR("Couldn't open pipe.\n");

    pid_t pid = fork();
    if (pid == -1) ERROR("Couldn't fork process.\n");
    if (pid == 0) {
        int read_fd = pipe_fds[0];
        if (close(pipe_fds[1]) == -1) exit(1);

        if (dup2(read_fd, STDIN_FILENO) == -1) exit(1);
        if (execvp("git", args) == -1) exit(1);
    }

    int write_fd = pipe_fds[1];
    if (close(pipe_fds[0]) == -1) ERROR("Couldn't close pipe.\n");

    for (size_t i = 0; i < patches->length; i++) {
        const char *patch = patches->data[i];
        ssize_t bytes = write(write_fd, patch, strlen(patch));
        if (bytes != ((ssize_t) strlen(patch))) ERROR("Couldn't write the entire patch.\n");
    }

    if (close(write_fd) == -1) ERROR("Couldn't close pipe.\n");

    int status;
    if (waitpid(pid, &status, 0) == -1) ERROR("Couldn't wait for child process.\n");

    return status;
}