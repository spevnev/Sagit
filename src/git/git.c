#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "utils/memory.h"
static const char *file_header_fmt = "diff --git %s %s\n--- %s\n+++ %s\n";
static const char *hunk_header_fmt = "@@ -%d,%d +%d,%d @@\n";
static const char *new_file_header_fmt = "diff --git %s %s\nnew file mode %o\n--- /dev/null\n+++ %s\n";
// It also modifies text by replacing delimiters with nulls.
    ASSERT(text != NULL);
    ASSERT(diff != NULL);
        ASSERT(strncmp(lines.data[i], "diff --git", 10) == 0);
        if (i == lines.length) break;
            ASSERT(lines.data[i][0] == '@');  // hunk header
    int ignore, start = 0, old_length = 0, new_length = 0;
    int matched = sscanf(hunk->header, hunk_header_fmt, &start, &old_length, &ignore, &new_length);
    ASSERT(matched >= 3);
    // handles (un)staging of partially staged FC_CREATED files
    // const char *file_src = strcmp(file->src, "/dev/null") ? file->src : file->dest;
    const char *file_src = file->src;

    size_t file_header_size = snprintf(NULL, 0, file_header_fmt, file_src, file->dest, file_src, file->dest);
    size_t hunk_header_size = snprintf(NULL, 0, hunk_header_fmt, start, old_length, start, new_length);
    ptr += snprintf(ptr, file_header_size + 1, file_header_fmt, file_src, file->dest, file_src, file->dest);
    ptr += snprintf(ptr, hunk_header_size + 1, hunk_header_fmt, start, old_length, start, new_length);
// Returns NULL in case patch doesn't contain any changes or doesn't need to be applied
    int ignore, start = 0, old_length = 0, new_length = 0;
    int matched = sscanf(hunk->header, hunk_header_fmt, &start, &old_length, &ignore, &new_length);
    ASSERT(matched >= 3);
    size_t patch_size = 1;  // space for '\0'
    if (new_length == 0) {
        // After unstaging this range the file will be left empty, thus in order
        // not to leave empty-yet-added files we have to unstage it
        ASSERT(file->change_type == FC_CREATED);
        git_unstage_file(file->dest + 2);
        free(patch_body);
        return NULL;
    }

    char *patch;
    size_t hunk_header_size = snprintf(NULL, 0, hunk_header_fmt, start, old_length, start, new_length);

    if (file->change_type != FC_CREATED) {
        assert(strcmp(file->src, "/dev/null") != 0);

        size_t file_header_size = snprintf(NULL, 0, file_header_fmt, file->src, file->dest, file->src, file->dest);
        patch = (char *) malloc(file_header_size + hunk_header_size + patch_size);

        ptr = patch;
        ptr += snprintf(ptr, file_header_size + 1, file_header_fmt, file->src, file->dest, file->src, file->dest);
    } else if (reverse) {  // Partial unstaging of untracked file requires src == dest
        size_t file_header_size = snprintf(NULL, 0, file_header_fmt, file->dest, file->dest, file->dest, file->dest);
        patch = (char *) malloc(file_header_size + hunk_header_size + patch_size);

        ptr = patch;
        ptr += snprintf(ptr, file_header_size + 1, file_header_fmt, file->dest, file->dest, file->dest, file->dest);
    } else {  // Partial staging of untracked file requires "new file mode"
        struct stat file_info = {0};
        if (stat(file->dest + 2, &file_info) == -1) ERROR("Unable to stat \"%s\".\n", file->dest + 2);

        size_t file_header_size = snprintf(NULL, 0, new_file_header_fmt, file->dest, file->dest, file_info.st_mode, file->dest);
        patch = (char *) malloc(file_header_size + hunk_header_size + patch_size);

        ptr = patch;
        ptr += snprintf(ptr, file_header_size + 1, new_file_header_fmt, file->dest, file->dest, file_info.st_mode, file->dest);
    }

    ptr += snprintf(ptr, hunk_header_size + 1, hunk_header_fmt, start, old_length, start, new_length);
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

int is_state_empty(State *state) { return state->unstaged.files.length == 0 && state->staged.files.length == 0; }
    ASSERT(state != NULL);
    char *raw_file_paths = gexecr(NULL, CMD_UNTRACKED);
    str_vec untracked_file_paths = split(raw_file_paths, '\n');

    ctxt_init(&state->untracked_ctxt);
    for (size_t i = 0; i < untracked_file_paths.length; i++) {
        File file = create_file_from_untracked(&state->untracked_ctxt, untracked_file_paths.data[i]);
        VECTOR_PUSH(&state->unstaged.files, file);
    }

    VECTOR_FREE(&untracked_file_paths);
    free(raw_file_paths);

    ASSERT(state != NULL);
    char *raw_file_paths = gexecr(NULL, CMD_UNTRACKED);
    str_vec untracked_file_paths = split(raw_file_paths, '\n');

    ctxt_reset(&state->untracked_ctxt);
    for (size_t i = 0; i < untracked_file_paths.length; i++) {
        File file = create_file_from_untracked(&state->untracked_ctxt, untracked_file_paths.data[i]);
        VECTOR_PUSH(&state->unstaged.files, file);
    }

    VECTOR_FREE(&untracked_file_paths);
    free(raw_file_paths);
