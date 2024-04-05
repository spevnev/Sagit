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

static void merge_files(const FileVec *old_files, FileVec *new_files) {
    for (size_t i = 0; i < new_files->length; i++) {
        File *new_file = &new_files->data[i];

        for (size_t j = 0; j < old_files->length; j++) {
            File *old_file = &old_files->data[j];
            if (strcmp(old_file->src, new_file->src) != 0) continue;

            for (size_t k = 0; k < new_file->hunks.length; k++) {
                Hunk *new_hunk = &new_file->hunks.data[k];

                for (size_t h = 0; h < old_file->hunks.length; h++) {
                    Hunk *old_hunk = &old_file->hunks.data[h];
                    if (strcmp(old_file->src, new_file->src) != 0) continue;

                    new_hunk->is_folded = old_hunk->is_folded;
                    break;
                }
            }

            new_file->is_folded = old_file->is_folded;
            break;
        }
    }
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

int is_state_empty(State *state) {
    assert(state != NULL);
    return state->untracked.files.length == 0 && state->unstaged.files.length == 0 && state->staged.files.length == 0;
}

void update_git_state(State *state) {
    assert(state != NULL);

    assert(state->untracked.raw != NULL);
    VECTOR_FREE(&state->untracked.files);
    free(state->untracked.raw);
    state->untracked.raw = git_exec(NULL, CMD("git", "ls-files", "--others", "--exclude-standard"));
    state->untracked.files = split(state->untracked.raw, '\n');

    char *unstaged_raw = git_exec(NULL, CMD("git", "diff"));
    FileVec unstaged_files = parse_diff(unstaged_raw);
    merge_files(&state->unstaged.files, &unstaged_files);
    VECTOR_FREE(&state->unstaged.files);
    free(state->unstaged.raw);
    state->unstaged.files = unstaged_files;
    state->unstaged.raw = unstaged_raw;

    char *staged_raw = git_exec(NULL, CMD("git", "diff", "--staged"));
    FileVec staged_files = parse_diff(staged_raw);
    merge_files(&state->staged.files, &staged_files);
    VECTOR_FREE(&state->staged.files);
    free(state->staged.raw);
    state->staged.files = staged_files;
    state->staged.raw = staged_raw;
}

void git_stage_file(char *file_path) {
    char *output = git_exec(NULL, CMD("git", "add", file_path));
    free(output);
}

void git_unstage_file(char *file_path) {
    char *output = git_exec(NULL, CMD("git", "rm", "--cached", file_path));
    free(output);
}