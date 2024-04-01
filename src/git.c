#include "git.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "error.h"
#include "vector.h"

DEFINE_VECTOR_TYPE(str_vec, char *);

#define INITIAL_BUFFER_SIZE 1024

#define NO_GIT_BINARY 100

#define CMD(...) ((char *const[]){__VA_ARGS__, NULL})

static char *git_exec(int *out_status, char *const *args) {
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

    int _status;
    int *status = out_status;
    if (out_status == NULL) status = &_status;

    if (waitpid(pid, status, 0) == -1) ERROR("Couldn't wait for child process.\n");
    if (close(read_fd) == -1) ERROR("Couldn't close pipe.\n");

    if (WEXITSTATUS(*status) == NO_GIT_BINARY) ERROR("Couldn't find git binary. Make sure it is in PATH.\n");
    if (out_status == NULL && WEXITSTATUS(_status) != 0) ERROR("Childs process exited with non-zero exit code.\n");

    return buffer;
}

// Lines are stored as pointers into the text, thus text must be free after lines.
// It also modifies text by replacing delimiters with nulls
static str_vec split(char *text, char delimiter) {
    str_vec lines = {0};

    char *line_start = text;
    for (char *ch = text; *ch; ch++) {
        if (*ch != delimiter) continue;

        *ch = '\0';
        VECTOR_PUSH(&lines, line_start);
        line_start = ch + 1;
    }
    if (*line_start != '\0') VECTOR_PUSH(&lines, line_start);

    return lines;
}

int is_git_initialized(void) {
    int status;
    char *output = git_exec(&status, CMD("git", "status"));
    free(output);

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int get_git_state(GitState *git) {
    char *untracked_raw = git_exec(NULL, CMD("git", "ls-files", "--others", "--exclude-standard"));
    str_vec untracked_files = split(untracked_raw, '\n');
    // TODO: store output in lines? struct Lines?

    //

    free(untracked_raw);
    free(untracked_files.data);
}