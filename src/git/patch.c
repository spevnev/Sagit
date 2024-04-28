#include "patch.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "error.h"
#include "git/git.h"
#include "git/state.h"
#include "vector.h"

static const char *NO_NEWLINE = "\\ No newline at end of file";

static const char *file_header_fmt = "diff --git %s %s\n--- a/%s\n+++ b/%s\n";
static const char *new_file_header_fmt = "diff --git %s %s\nnew file mode %o\n--- /dev/null\n+++ b/%s\n";
static const char *hunk_header_fmt = "@@ -%d,%d +%d,%d @@\n";

typedef struct {
    int start;
    int old_length;
    int new_length;
} HunkHeader;

static HunkHeader parse_hunk_header(const char *raw) {
    ASSERT(raw != NULL);

    HunkHeader header = {-1, -1, -1};
    int a = 0, b = 0, offset = 0;

    if (sscanf(raw, "@@ -%d,%d %n", &a, &b, &offset) == 2) {
        header.start = a;
        header.old_length = b;
    } else if (sscanf(raw, "@@ -%d %n", &a, &offset) == 1) {
        header.old_length = a;
    } else ERROR("Unable to parse hunk header: \"%s\".\n", raw);

    if (sscanf(raw + offset, "+%d,%d @@", &a, &b) == 2) {
        if (header.start == -1) header.start = a;
        header.new_length = b;
    } else if (sscanf(raw + offset, "+%d @@", &a) == 1) {
        header.new_length = a;
    } else ERROR("Unable to parse hunk header: \"%s\".\n", raw);

    ASSERT(header.start != -1 && header.old_length != -1 && header.new_length != -1);
    return header;
}

char *create_patch_from_hunk(const File *file, const Hunk *hunk) {
    ASSERT(file != NULL && hunk != NULL);

    HunkHeader header = parse_hunk_header(hunk->header);

    size_t hunk_length = 0;
    for (size_t i = 0; i < hunk->lines.length; i++) hunk_length += strlen(hunk->lines.data[i]) + 1;

    size_t file_header_size = snprintf(NULL, 0, file_header_fmt, file->src, file->dst, file->src, file->dst);
    size_t hunk_header_size = snprintf(NULL, 0, hunk_header_fmt, header.start, header.old_length, header.start, header.new_length);
    char *patch = (char *) malloc(file_header_size + hunk_header_size + hunk_length + 1);
    if (patch == NULL) OUT_OF_MEMORY();

    char *ptr = patch;
    ptr += snprintf(ptr, file_header_size + 1, file_header_fmt, file->src, file->dst, file->src, file->dst);
    ptr += snprintf(ptr, hunk_header_size + 1, hunk_header_fmt, header.start, header.old_length, header.start, header.new_length);

    for (size_t i = 0; i < hunk->lines.length; i++) {
        size_t len = strlen(hunk->lines.data[i]);
        memcpy(ptr, hunk->lines.data[i], len);
        ptr += len;
        *ptr++ = '\n';
    }
    *ptr = '\0';

    return patch;
}

char *create_patch_from_range(const File *file, const Hunk *hunk, size_t range_start, size_t range_end, bool reverse) {
    ASSERT(file != NULL && hunk != NULL);
    ASSERT(hunk->lines.length >= 1);

    HunkHeader header = parse_hunk_header(hunk->header);

    size_t patch_size = 1;  // space for '\0'
    for (size_t i = 0; i < hunk->lines.length; i++) patch_size += strlen(hunk->lines.data[i]) + 1;
    char *patch_body = (char *) malloc(patch_size);
    if (patch_body == NULL) OUT_OF_MEMORY();

    bool has_changes = false;
    bool has_unstaged_changes = false;
    char *ptr = patch_body;
    for (size_t i = 0; i < hunk->lines.length; i++) {
        const char *line = hunk->lines.data[i];

        if (i < range_start || i > range_end) {
            if (line[0] == '+' || line[0] == '-') has_unstaged_changes = true;

            if (!reverse && line[0] == '+') {
                // skip to prevent it from being applied
                header.new_length--;
                continue;
            } else if (reverse && line[0] == '-') {
                // skip because it has already been applied
                header.old_length--;
                continue;
            }
        }

        size_t len = strlen(line);
        memcpy(ptr, line, len);
        if (i < range_start || i > range_end) {
            if (!reverse && line[0] == '-') {
                // prevent it from being applied
                *ptr = ' ';
                header.new_length++;
            } else if (reverse && line[0] == '+') {
                // "apply", because it has already been applied
                *ptr = ' ';
                header.old_length++;
            }
        } else if (line[0] == '-' || line[0] == '+') has_changes = true;

        ptr += len;
        *ptr++ = '\n';
    }
    *ptr = '\0';

    if (!reverse) {
        // Handle partial staging of files/hunks with "\ No newline at end of file"
        if (strcmp(hunk->lines.data[hunk->lines.length - 1], NO_NEWLINE) == 0 && has_unstaged_changes) {
            ASSERT(hunk->lines.length >= 2);
            bool is_last_staged = hunk->lines.data[hunk->lines.length - 2][0] == ' ' || range_end >= hunk->lines.length - 2;
            if (!is_last_staged) *(ptr - strlen(NO_NEWLINE) - 1) = '\0';
        }
    }

    if ((!reverse && header.new_length == 0) || (reverse && header.old_length == 0)) {
        // This patch will leave file empty, so we should unstage it
        git_unstage_file(file->dst);
        free(patch_body);
        return NULL;
    }

    if (!has_changes) {
        free(patch_body);
        return NULL;
    }

    char *patch;
    size_t hunk_header_size = snprintf(NULL, 0, hunk_header_fmt, header.start, header.old_length, header.start, header.new_length);

    if (file->change_type != FC_CREATED) {
        size_t file_header_size = snprintf(NULL, 0, file_header_fmt, file->src, file->dst, file->src, file->dst);
        patch = (char *) malloc(file_header_size + hunk_header_size + patch_size);
        if (patch == NULL) OUT_OF_MEMORY();

        ptr = patch;
        ptr += snprintf(ptr, file_header_size + 1, file_header_fmt, file->src, file->dst, file->src, file->dst);
    } else if (reverse) {  // Partial unstaging of untracked file requires src == dst
        size_t file_header_size = snprintf(NULL, 0, file_header_fmt, file->dst, file->dst, file->dst, file->dst);
        patch = (char *) malloc(file_header_size + hunk_header_size + patch_size);
        if (patch == NULL) OUT_OF_MEMORY();

        ptr = patch;
        ptr += snprintf(ptr, file_header_size + 1, file_header_fmt, file->dst, file->dst, file->dst, file->dst);
    } else {  // Partial staging of untracked file requires "new file mode"
        struct stat file_info = {0};
        if (stat(file->dst, &file_info) == -1) ERROR("Unable to stat \"%s\": %s.\n", file->dst, strerror(errno));

        size_t file_header_size = snprintf(NULL, 0, new_file_header_fmt, file->dst, file->dst, file_info.st_mode, file->dst);
        patch = (char *) malloc(file_header_size + hunk_header_size + patch_size);
        if (patch == NULL) OUT_OF_MEMORY();

        ptr = patch;
        ptr += snprintf(ptr, file_header_size + 1, new_file_header_fmt, file->dst, file->dst, file_info.st_mode, file->dst);
    }

    ptr += snprintf(ptr, hunk_header_size + 1, hunk_header_fmt, header.start, header.old_length, header.start, header.new_length);
    memcpy(ptr, patch_body, patch_size);

    free(patch_body);
    return patch;
}
