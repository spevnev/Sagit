#include "git.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "git/exec.h"
#include "git/patch.h"
#include "git/state.h"
#include "utils/error.h"
#include "utils/memory.h"
#include "utils/vector.h"

// clang-format off
static char *const CMD_UNTRACKED[]     = {"git", "ls-files", "--others", "--exclude-standard"};
static char *const CMD_UNSTAGED[]      = {"git", "diff"};
static char *const CMD_STAGED[]        = {"git", "diff", "--staged"};

// "-" means to read from stdin instead of file
static char *const CMD_APPLY[]         = {"git", "apply", "--cached", "-"};
static char *const CMD_APPLY_REVERSE[] = {"git", "apply", "--cached", "--reverse", "-"};
// clang-format on

static const char *diff_header_size_fmt = "diff --git a/%n%*s%n b/%n%*s%n";

// Lines are stored as pointers into the text, thus text must be free after lines.
// It also modifies text by replacing delimiters with nulls.
static str_vec split(char *text, char delimiter) {
    ASSERT(text != NULL);

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
    ASSERT(diff != NULL);

    str_vec lines = split(diff, '\n');
    FileVec files = {0};
    size_t i = 0;
    while (i < lines.length) {
        int src_begin, src_end, dst_begin, dst_end;
        if (sscanf(lines.data[i], diff_header_size_fmt, &src_begin, &src_end, &dst_begin, &dst_end) != 0) ERROR("Unable to parse file diff header.\n");

        File file = {0};
        file.is_folded = true;
        file.change_type = FC_MODIFIED;
        file.src = lines.data[i] + src_begin;
        lines.data[i][src_end] = '\0';
        file.dst = lines.data[i] + dst_begin;
        lines.data[i][dst_end] = '\0';

        while (i < lines.length && lines.data[i][0] != '@') {
            if (strncmp(lines.data[i], "Binary files ", 13) == 0) file.is_binary = true;
            if (strncmp(lines.data[i], "new file mode", 13) == 0) file.change_type = FC_CREATED;
            if (strncmp(lines.data[i], "deleted file mode", 17) == 0) file.change_type = FC_DELETED;
            if (strncmp(lines.data[i], "--- ", 4) == 0) {
                if (file.change_type == FC_CREATED) ASSERT(strcmp(lines.data[i] + 4, "/dev/null") == 0);
                else ASSERT(strcmp(lines.data[i] + 6, file.src) == 0);
            }
            if (strncmp(lines.data[i], "+++ ", 4) == 0) {
                if (file.change_type == FC_DELETED) ASSERT(strcmp(lines.data[i] + 4, "/dev/null") == 0);
                else ASSERT(strcmp(lines.data[i] + 6, file.dst) == 0);
            }
            i++;
        }
        if (strcmp(file.src, file.dst) != 0) file.change_type = FC_RENAMED;

        while (i < lines.length && lines.data[i][0] == '@') {
            Hunk hunk = {0};
            hunk.header = lines.data[i++];
            while (i < lines.length && lines.data[i][0] != '@' && lines.data[i][0] != 'd') VECTOR_PUSH(&hunk.lines, lines.data[i++]);
            VECTOR_PUSH(&file.hunks, hunk);
        }

        VECTOR_PUSH(&files, file);
    }

    VECTOR_FREE(&lines);
    return files;
}

static File create_file_from_untracked(MemoryContext *ctxt, const char *file_path) {
    ASSERT(ctxt != NULL && file_path != NULL);

    static const char *hunk_header_fmt = "@@ -0,0 +0,%d @@";

    struct stat file_info = {0};
    if (stat(file_path, &file_info) == -1) ERROR("Unable to stat \"%s\": %s.\n", file_path, strerror(errno));
    size_t size = file_info.st_size;

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) ERROR("Unable to open \"%s\": %s.\n", file_path, strerror(errno));

    // TODO: read in blocks instead?
    char *buffer = (char *) malloc(size);
    if (buffer == NULL) OUT_OF_MEMORY();
    if (read(fd, buffer, size) != (ssize_t) size) ERROR("Unable to read \"%s\": %s.\n", file_path, strerror((errno)));
    close(fd);

    size_t offset = 0;
    str_vec lines = {0};
    for (size_t i = 0; i <= size; i++) {
        if (i < size && buffer[i] != '\n') continue;

        size_t length = i - offset;
        char *line = (char *) ctxt_alloc(ctxt, 1 + length + 1);

        line[0] = '+';
        memcpy(line + 1, buffer + offset, length);
        line[1 + length] = '\0';

        VECTOR_PUSH(&lines, line);
        offset = i + 1;
    }
    free(buffer);

    size_t hunk_header_size = snprintf(NULL, 0, hunk_header_fmt, lines.length);
    char *hunk_header = (char *) ctxt_alloc(ctxt, hunk_header_size + 1);
    snprintf(hunk_header, hunk_header_size + 1, hunk_header_fmt, lines.length);

    HunkVec hunks = {0};
    Hunk hunk = {0, hunk_header, lines};
    VECTOR_PUSH(&hunks, hunk);

    size_t length = strlen(file_path);
    char *dst_path = (char *) ctxt_alloc(ctxt, length + 1);
    memcpy(dst_path, file_path, length);
    dst_path[length] = '\0';

    int exit_code = gexec(CMD("git", "grep", "-I", "--name-only", "--untracked", "-e", ".", "--", dst_path));
    bool is_binary = exit_code != 0;

    return (File){true, is_binary, FC_CREATED, dst_path, dst_path, hunks};
}

static void merge_files(const FileVec *old_files, FileVec *new_files) {
    ASSERT(old_files != NULL && new_files != NULL);

    for (size_t i = 0; i < new_files->length; i++) {
        File *new_file = &new_files->data[i];

        for (size_t j = 0; j < old_files->length; j++) {
            const File *old_file = &old_files->data[j];
            if (strcmp(old_file->src, new_file->src) != 0) continue;

            for (size_t k = 0; k < new_file->hunks.length; k++) {
                Hunk *new_hunk = &new_file->hunks.data[k];

                for (size_t h = 0; h < old_file->hunks.length; h++) {
                    const Hunk *old_hunk = &old_file->hunks.data[h];
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

bool is_git_initialized(void) { return gexec(CMD("git", "status")) == 0; }

bool is_state_empty(State *state) {
    ASSERT(state != NULL);
    return state->unstaged.files.length == 0 && state->staged.files.length == 0;
}

bool is_ignored(char *file_path) {
    ASSERT(file_path != NULL);
    return gexec(CMD("git", "check-ignore", file_path)) == 0;
}

void get_git_state(State *state) {
    ASSERT(state != NULL);

    state->unstaged.raw = gexecr(CMD_UNSTAGED);
    state->unstaged.files = parse_diff(state->unstaged.raw);

    // TODO: create a separate function
    char *raw_file_paths = gexecr(CMD_UNTRACKED);
    str_vec untracked_file_paths = split(raw_file_paths, '\n');

    ctxt_init(&state->untracked_ctxt);
    for (size_t i = 0; i < untracked_file_paths.length; i++) {
        File file = create_file_from_untracked(&state->untracked_ctxt, untracked_file_paths.data[i]);
        VECTOR_PUSH(&state->unstaged.files, file);
    }

    VECTOR_FREE(&untracked_file_paths);
    free(raw_file_paths);

    state->staged.raw = gexecr(CMD_STAGED);
    state->staged.files = parse_diff(state->staged.raw);
}

void update_git_state(State *state) {
    ASSERT(state != NULL);

    char *unstaged_raw = gexecr(CMD_UNSTAGED);
    FileVec unstaged_files = parse_diff(unstaged_raw);
    merge_files(&state->unstaged.files, &unstaged_files);
    free_files(&state->unstaged.files);
    free(state->unstaged.raw);
    state->unstaged.files = unstaged_files;
    state->unstaged.raw = unstaged_raw;

    char *raw_file_paths = gexecr(CMD_UNTRACKED);
    str_vec untracked_file_paths = split(raw_file_paths, '\n');

    ctxt_reset(&state->untracked_ctxt);
    for (size_t i = 0; i < untracked_file_paths.length; i++) {
        File file = create_file_from_untracked(&state->untracked_ctxt, untracked_file_paths.data[i]);
        VECTOR_PUSH(&state->unstaged.files, file);
    }

    VECTOR_FREE(&untracked_file_paths);
    free(raw_file_paths);

    char *staged_raw = gexecr(CMD_STAGED);
    FileVec staged_files = parse_diff(staged_raw);
    merge_files(&state->staged.files, &staged_files);
    free_files(&state->staged.files);
    free(state->staged.raw);
    state->staged.files = staged_files;
    state->staged.raw = staged_raw;
}

void git_stage_file(const char *file_path) {
    ASSERT(file_path != NULL);
    if (gexec(CMD("git", "add", (char *) file_path)) != 0) ERROR("Unable to stage file.\n");
}

void git_unstage_file(const char *file_path) {
    ASSERT(file_path != NULL);
    if (gexec(CMD("git", "restore", "--staged", (char *) file_path)) != 0) ERROR("Unable to unstage file.\n");
}

void git_stage_hunk(const File *file, const Hunk *hunk) {
    ASSERT(file != NULL && hunk != NULL);

    char *patch = create_patch_from_hunk(file, hunk);
    if (gexecw(CMD_APPLY, patch) != 0) {
        DUMP_PATCH(patch);
        ERROR("Unable to stage the hunk. Failed patch written to \"%s\".\n", FAILED_PATCH_PATH);
    }
    free(patch);
}

void git_unstage_hunk(const File *file, const Hunk *hunk) {
    ASSERT(file != NULL && hunk != NULL);

    char *patch = create_patch_from_hunk(file, hunk);
    if (gexecw(CMD_APPLY_REVERSE, patch) != 0) {
        DUMP_PATCH(patch);
        ERROR("Unable to unstage the hunk. Failed patch written to \"%s\".\n", FAILED_PATCH_PATH);
    }
    free(patch);
}

void git_stage_range(const File *file, const Hunk *hunk, int range_start, int range_end) {
    ASSERT(file != NULL && hunk != NULL);

    char *patch = create_patch_from_range(file, hunk, range_start, range_end, 0);
    if (patch == NULL) return;
    if (gexecw(CMD_APPLY, patch) != 0) {
        DUMP_PATCH(patch);
        ERROR("Unable to stage the range. Failed patch written to \"%s\".\n", FAILED_PATCH_PATH);
    }
    free(patch);
}

void git_unstage_range(const File *file, const Hunk *hunk, int range_start, int range_end) {
    ASSERT(file != NULL && hunk != NULL);

    char *patch = create_patch_from_range(file, hunk, range_start, range_end, 1);
    if (patch == NULL) return;
    if (gexecw(CMD_APPLY_REVERSE, patch) != 0) {
        DUMP_PATCH(patch);
        ERROR("Unable to unstage the range. Failed patch written to \"%s\".\n", FAILED_PATCH_PATH);
    }
    free(patch);
}