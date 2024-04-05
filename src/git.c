#include "git.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "error.h"
#include "exec.h"
#include "state.h"
#include "vector.h"

// Lines are stored as pointers into the text, thus text must be free after lines.
// It also modifies text by replacing delimiters with nulls
static str_vec split(char *text, char delimiter) {
    assert(text != NULL);

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
    assert(diff != NULL);

    str_vec lines = split(diff, '\n');
    FileVec files = {0};
    size_t i = 0;
    while (i < lines.length) {
        assert(strncmp(lines.data[i], "diff --git", 10) == 0 && i + 1 < lines.length);
        i++;
        if (strncmp(lines.data[i], "new file mode", 13) == 0) i++;
        assert(strncmp(lines.data[i], "index ", 6) == 0 && i + 1 < lines.length);
        i++;

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

void get_git_state(State *state) {
    assert(state != NULL);

    state->untracked.raw = git_exec(NULL, CMD("git", "ls-files", "--others", "--exclude-standard"));
    state->untracked.files = split(state->untracked.raw, '\n');

    state->unstaged.raw = git_exec(NULL, CMD("git", "diff"));
    state->unstaged.files = parse_diff(state->unstaged.raw);

    state->staged.raw = git_exec(NULL, CMD("git", "diff", "--staged"));
    state->staged.files = parse_diff(state->staged.raw);
}


}

int is_state_empty(State *state) {
    assert(state != NULL);
    return state->untracked.files.length == 0 && state->unstaged.files.length == 0 && state->staged.files.length == 0;
}