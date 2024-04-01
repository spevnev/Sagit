#include "git.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "error.h"

#define INITIAL_BUFFER_SIZE 1024

#define NO_GIT_BINARY 100

char *git_exec(int *status, char *const *args) {
    assert(status != NULL);

    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) ERROR("Couldn't open pipe.\n");

    pid_t pid = fork();
    if (pid == -1) ERROR("Couldn't fork process.\n");

    if (pid == 0) {
        int write_fd = pipe_fds[1];
        if (close(pipe_fds[0]) == -1) exit(1);

        if (dup2(write_fd, STDOUT_FILENO) == -1) exit(1);
        if (dup2(write_fd, STDERR_FILENO) == -1) exit(1);

        if (execvp("git", args) == -1 && errno == ENOENT) exit(NO_GIT_BINARY);
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

    if (waitpid(pid, status, 0) == -1) ERROR("Couldn't wait for child process.\n");
    if (WEXITSTATUS(*status) == NO_GIT_BINARY) ERROR("Couldn't find git binary. Make sure it is in PATH.\n");

    if (close(read_fd) == -1) ERROR("Couldn't close pipe.\n");

    return buffer;
}

int is_git_initialized(void) {
    int status;
    char *const cmd[] = {"git", "status", NULL};
    char *output = git_exec(&status, cmd);
    free(output);

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int get_git_state(GitState *git) {}