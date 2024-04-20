#include "git.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "git/exec.h"
#include "git/state.h"
#include "utils/error.h"
#include "utils/memory.h"
#include "utils/vector.h"

// clang-format off
static char *const CMD_UNTRACKED[]     = {"git", "ls-files", "--others", "--exclude-standard"};
static char *const CMD_UNSTAGED[]      = {"git", "diff"};
static char *const CMD_STAGED[]        = {"git", "diff", "--staged"};

static char *const CMD_APPLY[]         = {"git", "apply", "--cached", "-"};
static char *const CMD_APPLY_REVERSE[] = {"git", "apply", "--cached", "--reverse", "-"};
// clang-format on

static const char *file_header_fmt = "diff --git %s %s\n--- %s\n+++ %s\n";
static const char *hunk_header_fmt = "@@ -%d,%d +%d,%d @@\n";
static const char *new_file_header_fmt = "diff --git %s %s\nnew file mode %o\n--- /dev/null\n+++ %s\n";

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
        // check diff header
        ASSERT(strncmp(lines.data[i], "diff --git", 10) == 0);

        // skip header
        while (i < lines.length && lines.data[i][0] != '-') i++;
        if (i == lines.length) break;

        File file = {0};
        file.is_folded = true;
        file.src = lines.data[i++] + 4;
        file.dst = lines.data[i++] + 4;

        if (strcmp(file.src + 2, file.dst + 2) == 0) file.change_type = FC_MODIFIED;
        else if (strcmp(file.src, "/dev/null") == 0) file.change_type = FC_CREATED;
        else if (strcmp(file.dst, "/dev/null") == 0) file.change_type = FC_DELETED;
        else file.change_type = FC_RENAMED;

        while (i < lines.length) {
            if (lines.data[i][0] == 'd') break;
            ASSERT(lines.data[i][0] == '@');  // hunk header

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

static char *create_patch_from_hunk(const File *file, const Hunk *hunk) {
    // Because we are staging only a single hunk it should start at the same position as the original file
    int ignore, start = 0, old_length = 0, new_length = 0;
    int matched = sscanf(hunk->header, hunk_header_fmt, &start, &old_length, &ignore, &new_length);
    ASSERT(matched >= 3);

    size_t hunk_length = 0;
    for (size_t i = 0; i < hunk->lines.length; i++) hunk_length += strlen(hunk->lines.data[i]) + 1;

    // handles (un)staging of partially staged FC_CREATED files
    // const char *file_src = strcmp(file->src, "/dev/null") ? file->src : file->dst;
    const char *file_src = file->src;

    size_t file_header_size = snprintf(NULL, 0, file_header_fmt, file_src, file->dst, file_src, file->dst);
    size_t hunk_header_size = snprintf(NULL, 0, hunk_header_fmt, start, old_length, start, new_length);
    char *patch = (char *) malloc(file_header_size + hunk_header_size + hunk_length + 1);

    char *ptr = patch;
    ptr += snprintf(ptr, file_header_size + 1, file_header_fmt, file_src, file->dst, file_src, file->dst);
    ptr += snprintf(ptr, hunk_header_size + 1, hunk_header_fmt, start, old_length, start, new_length);

    for (size_t i = 0; i < hunk->lines.length; i++) {
        size_t len = strlen(hunk->lines.data[i]);
        memcpy(ptr, hunk->lines.data[i], len);
        ptr += len;
        *ptr++ = '\n';
    }
    *ptr = '\0';

    return patch;
}

// Returns NULL in case patch doesn't contain any changes or doesn't need to be applied
static char *create_patch_from_range(const File *file, const Hunk *hunk, size_t range_start, size_t range_end, bool reverse) {
    // Because we are staging only a single hunk it should start at the same position as the original file
    int ignore, start = 0, old_length = 0, new_length = 0;
    int matched = sscanf(hunk->header, hunk_header_fmt, &start, &old_length, &ignore, &new_length);
    ASSERT(matched >= 3);

    size_t patch_size = 1;  // space for '\0'
    for (size_t i = 0; i < hunk->lines.length; i++) patch_size += strlen(hunk->lines.data[i]) + 1;
    char *patch_body = (char *) malloc(patch_size);

    bool has_changes = false;
    char *ptr = patch_body;
    for (size_t i = 0; i < hunk->lines.length; i++) {
        const char *str = hunk->lines.data[i];
        if (i < range_start || i > range_end) {
            if (!reverse && str[0] == '+') {
                // skip to prevent it from being applied
                new_length--;
                continue;
            } else if (reverse && str[0] == '-') {
                // skip because it has already been applied
                old_length--;
                continue;
            }
        }

        size_t len = strlen(str);
        memcpy(ptr, str, len);
        if (i < range_start || i > range_end) {
            if (!reverse && str[0] == '-') {
                // prevent it from being applied
                *ptr = ' ';
                new_length++;
            } else if (reverse && str[0] == '+') {
                // "apply", because it has already been applied
                *ptr = ' ';
                old_length++;
            }
        } else {
            if (str[0] == '-' || str[0] == '+') has_changes = true;
        }

        ptr += len;
        *ptr++ = '\n';
    }
    *ptr = '\0';

    if (new_length == 0) {
        // After unstaging this range the file will be left empty, thus in order
        // not to leave empty-yet-added files we have to unstage it
        ASSERT(file->change_type == FC_CREATED);
        git_unstage_file(file->dst + 2);
        free(patch_body);
        return NULL;
    }

    if (!has_changes) {
        free(patch_body);
        return NULL;
    }

    char *patch;
    size_t hunk_header_size = snprintf(NULL, 0, hunk_header_fmt, start, old_length, start, new_length);

    if (file->change_type != FC_CREATED) {
        assert(strcmp(file->src, "/dev/null") != 0);

        size_t file_header_size = snprintf(NULL, 0, file_header_fmt, file->src, file->dst, file->src, file->dst);
        patch = (char *) malloc(file_header_size + hunk_header_size + patch_size);

        ptr = patch;
        ptr += snprintf(ptr, file_header_size + 1, file_header_fmt, file->src, file->dst, file->src, file->dst);
    } else if (reverse) {  // Partial unstaging of untracked file requires src == dst
        size_t file_header_size = snprintf(NULL, 0, file_header_fmt, file->dst, file->dst, file->dst, file->dst);
        patch = (char *) malloc(file_header_size + hunk_header_size + patch_size);

        ptr = patch;
        ptr += snprintf(ptr, file_header_size + 1, file_header_fmt, file->dst, file->dst, file->dst, file->dst);
    } else {  // Partial staging of untracked file requires "new file mode"
        struct stat file_info = {0};
        if (stat(file->dst + 2, &file_info) == -1) ERROR("Unable to stat \"%s\".\n", file->dst + 2);

        size_t file_header_size = snprintf(NULL, 0, new_file_header_fmt, file->dst, file->dst, file_info.st_mode, file->dst);
        patch = (char *) malloc(file_header_size + hunk_header_size + patch_size);

        ptr = patch;
        ptr += snprintf(ptr, file_header_size + 1, new_file_header_fmt, file->dst, file->dst, file_info.st_mode, file->dst);
    }

    ptr += snprintf(ptr, hunk_header_size + 1, hunk_header_fmt, start, old_length, start, new_length);

    memcpy(ptr, patch_body, patch_size);
    free(patch_body);

    return patch;
}

static File create_file_from_untracked(MemoryContext *ctxt, const char *file_path) {
    static const char *hunk_header_fmt = "@@ -0,0 +0,%d @@";

    struct stat file_info = {0};
    if (stat(file_path, &file_info) == -1) ERROR("Unable to stat \"%s\".\n", file_path);
    size_t size = file_info.st_size;

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) ERROR("Unable to open \"%s\".\n", file_path);

    // TODO: read in blocks instead?
    char *buffer = (char *) malloc(size);
    if (read(fd, buffer, size) != (ssize_t) size) ERROR("Unable to read \"%s\".\n", file_path);
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
    char *dst_path = (char *) ctxt_alloc(ctxt, 2 + length + 1);
    dst_path[0] = 'b';
    dst_path[1] = '/';
    memcpy(dst_path + 2, file_path, length);
    dst_path[2 + length] = '\0';

    return (File){1, FC_CREATED, "/dev/null", dst_path, hunks};
}

bool is_git_initialized(void) {
    int exit_code;
    char *output = gexecr(&exit_code, CMD("git", "status"));
    free(output);

    return WIFEXITED(exit_code) && WEXITSTATUS(exit_code) == 0;
}

bool is_state_empty(State *state) { return state->unstaged.files.length == 0 && state->staged.files.length == 0; }

bool is_ignored(char *file_path) {
    int exit_code = gexec(CMD("git", "check-ignore", file_path));
    return exit_code == 0;
}

void get_git_state(State *state) {
    ASSERT(state != NULL);

    state->unstaged.raw = gexecr(NULL, CMD_UNSTAGED);
    state->unstaged.files = parse_diff(state->unstaged.raw);

    char *raw_file_paths = gexecr(NULL, CMD_UNTRACKED);
    str_vec untracked_file_paths = split(raw_file_paths, '\n');

    ctxt_init(&state->untracked_ctxt);
    for (size_t i = 0; i < untracked_file_paths.length; i++) {
        File file = create_file_from_untracked(&state->untracked_ctxt, untracked_file_paths.data[i]);
        VECTOR_PUSH(&state->unstaged.files, file);
    }

    VECTOR_FREE(&untracked_file_paths);
    free(raw_file_paths);

    state->staged.raw = gexecr(NULL, CMD_STAGED);
    state->staged.files = parse_diff(state->staged.raw);
}

void update_git_state(State *state) {
    ASSERT(state != NULL);

    char *unstaged_raw = gexecr(NULL, CMD_UNSTAGED);
    FileVec unstaged_files = parse_diff(unstaged_raw);
    merge_files(&state->unstaged.files, &unstaged_files);
    free_files(&state->unstaged.files);
    free(state->unstaged.raw);
    state->unstaged.files = unstaged_files;
    state->unstaged.raw = unstaged_raw;

    char *raw_file_paths = gexecr(NULL, CMD_UNTRACKED);
    str_vec untracked_file_paths = split(raw_file_paths, '\n');

    ctxt_reset(&state->untracked_ctxt);
    for (size_t i = 0; i < untracked_file_paths.length; i++) {
        File file = create_file_from_untracked(&state->untracked_ctxt, untracked_file_paths.data[i]);
        VECTOR_PUSH(&state->unstaged.files, file);
    }

    VECTOR_FREE(&untracked_file_paths);
    free(raw_file_paths);

    char *staged_raw = gexecr(NULL, CMD_STAGED);
    FileVec staged_files = parse_diff(staged_raw);
    merge_files(&state->staged.files, &staged_files);
    free_files(&state->staged.files);
    free(state->staged.raw);
    state->staged.files = staged_files;
    state->staged.raw = staged_raw;
}

void git_stage_file(const char *file_path) { gexec(CMD("git", "add", (char *) file_path)); }

void git_unstage_file(const char *file_path) { gexec(CMD("git", "restore", "--staged", (char *) file_path)); }

void git_stage_hunk(const File *file, const Hunk *hunk) {
    char *patch = create_patch_from_hunk(file, hunk);
    if (gexecw(CMD_APPLY, patch) != 0) ERROR("Unable to stage the hunk.\n");
    free(patch);
}

void git_unstage_hunk(const File *file, const Hunk *hunk) {
    char *patch = create_patch_from_hunk(file, hunk);
    if (gexecw(CMD_APPLY_REVERSE, patch) != 0) ERROR("Unable to unstage the hunk.\n");
    free(patch);
}

void git_stage_range(const File *file, const Hunk *hunk, int range_start, int range_end) {
    char *patch = create_patch_from_range(file, hunk, range_start, range_end, 0);
    if (patch == NULL) return;
    if (gexecw(CMD_APPLY, patch) != 0) ERROR("Unable to stage the range.\n");
    free(patch);
}

void git_unstage_range(const File *file, const Hunk *hunk, int range_start, int range_end) {
    char *patch = create_patch_from_range(file, hunk, range_start, range_end, 1);
    if (patch == NULL) return;
    if (gexecw(CMD_APPLY_REVERSE, patch) != 0) ERROR("Unable to unstage the range.\n");
    free(patch);
}