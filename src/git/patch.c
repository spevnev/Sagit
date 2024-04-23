#include "patch.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "git/git.h"
#include "git/state.h"
#include "utils/error.h"
#include "utils/memory.h"
#include "utils/vector.h"

static const char *file_header_fmt = "diff --git %s %s\n--- %s\n+++ %s\n";
static const char *hunk_header_fmt = "@@ -%d,%d +%d,%d @@\n";
static const char *new_file_header_fmt = "diff --git %s %s\nnew file mode %o\n--- /dev/null\n+++ %s\n";

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

File create_file_from_untracked(MemoryContext *ctxt, const char *file_path) {
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
    char *dst_path = (char *) ctxt_alloc(ctxt, 2 + length + 1);
    dst_path[0] = 'b';
    dst_path[1] = '/';
    memcpy(dst_path + 2, file_path, length);
    dst_path[2 + length] = '\0';

    return (File){1, FC_CREATED, "/dev/null", dst_path, hunks};
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
        git_unstage_file(file->dst + 2);
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
        ASSERT(strcmp(file->src, "/dev/null") != 0);

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
        if (stat(file->dst + 2, &file_info) == -1) ERROR("Unable to stat \"%s\".\n", file->dst + 2);

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
