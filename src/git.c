#include "git.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "error.h"
#include "state.h"
#include "vector.h"

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

static FileVec parse_diff(char *diff) {
    str_vec lines = split(diff, '\n');
    FileVec files = {0};

    size_t i = 0;
    while (i < lines.length) {
        assert(strncmp(lines.data[i], "diff --git", 10) == 0 && i + 3 < lines.length);  // file header

        // skip diff and index lines
        i += 2;

        File file = {0};
        file.is_folded = 1;
        file.src = lines.data[i++] + 4;
        file.dest = lines.data[i++] + 4;

        while (i < lines.length) {
            if (lines.data[i][0] == 'd') break;
            assert(lines.data[i][0] == '@');  // hunk header

            Hunk hunk = {0};
            hunk.header = lines.data[i++];

            while (i < lines.length) {
                if (lines.data[i][0] == '@' || lines.data[i][0] == 'd') break;
                VECTOR_PUSH(&hunk.lines, lines.data[i++]);
            }

            VECTOR_PUSH(&file.hunks, hunk);
        }

        VECTOR_PUSH(&files, file);
    }

    VECTOR_FREE(&lines);
    return files;
}

int is_git_initialized(void) {
    int status;
    char *output = git_exec(&status, CMD("git", "status"));
    free(output);

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int get_git_state(State *state) {
    state->untracked.raw = git_exec(NULL, CMD("git", "ls-files", "--others", "--exclude-standard"));
    state->untracked.files = split(state->untracked.raw, '\n');

    state->unstaged.raw = git_exec(NULL, CMD("git", "diff"));
    state->unstaged.files = parse_diff(state->unstaged.raw);

    state->staged.raw = git_exec(NULL, CMD("git", "diff", "--staged"));
    state->staged.files = parse_diff(state->staged.raw);

    return 0;
}
