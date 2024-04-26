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

    HunkHeader header = {0};
    int a = 0, b = 0, pos = 0;

    if (sscanf(raw, "@@ -%d%n", &a, &pos) != 1) ERROR("Unable to parse hunk header: \"%s\".\n", raw);
    raw += pos;

    if (sscanf(raw, ",%d %n", &b, &pos) == 1) {
        header.old_length = b;
        raw += pos;
    } else {
        header.old_length = a;
        raw++;
    }

    if (sscanf(raw, "+%d,%d @@", &a, &b) != 2) ERROR("Unable to parse hunk header: \"%s\".\n", raw);
    header.start = a;
    header.new_length = b;

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

    HunkHeader header = parse_hunk_header(hunk->header);

    size_t patch_size = 1;  // space for '\0'
    for (size_t i = 0; i < hunk->lines.length; i++) patch_size += strlen(hunk->lines.data[i]) + 1;
    char *patch_body = (char *) malloc(patch_size);
    if (patch_body == NULL) OUT_OF_MEMORY();

    bool has_changes = false;
    char *ptr = patch_body;
    for (size_t i = 0; i < hunk->lines.length; i++) {
        const char *str = hunk->lines.data[i];
        if (i < range_start || i > range_end) {
            if (!reverse && str[0] == '+') {
                // skip to prevent it from being applied
                header.new_length--;
                continue;
            } else if (reverse && str[0] == '-') {
                // skip because it has already been applied
                header.old_length--;
                continue;
            }
        }

        size_t len = strlen(str);
        memcpy(ptr, str, len);
        if (i < range_start || i > range_end) {
            if (!reverse && str[0] == '-') {
                // prevent it from being applied
                *ptr = ' ';
                header.new_length++;
            } else if (reverse && str[0] == '+') {
                // "apply", because it has already been applied
                *ptr = ' ';
                header.old_length++;
            }
        } else {
            if (str[0] == '-' || str[0] == '+') has_changes = true;
        }

        ptr += len;
        *ptr++ = '\n';
    }
    *ptr = '\0';

    if (header.new_length == 0) {
        // After unstaging this range the file will be left empty, thus in order
        // not to leave empty-yet-added files we have to unstage it
        ASSERT(file->change_type == FC_CREATED);
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
