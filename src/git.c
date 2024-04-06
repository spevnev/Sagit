#include <stdlib.h>
static const char *file_diff_header = "git --diff %s %s\n--- %s\n+++ %s\n";
static const char *hunk_diff_header = "@@ -%d,%d +%d,%d @@\n";

        // check git diff header

        // skip extended header lines
        for (int j = 0; j < 4; j++) {
            // clang-format off
            if (strncmp(lines.data[i], "new file mode",     13) == 0 ||
                strncmp(lines.data[i], "deleted file mode", 17) == 0 ||
                strncmp(lines.data[i], "index",              5) == 0 ||
                strncmp(lines.data[i], "mode",               4) == 0) {
                i++;
                assert(i < lines.length);
            }
            // clang-format on
        }
}

void git_stage_hunk(const File *file, const Hunk *hunk) {
    int old_start, old_length, new_start, new_length;
    int matched = sscanf(hunk->header, hunk_diff_header, &old_start, &old_length, &new_start, &new_length);
    assert(matched == 4);

    // Because we are staging only a single hunk it should start at the same position as the original file
    new_start = old_start;

    size_t hunk_length = 0;
    for (size_t i = 0; i < hunk->lines.length; i++) hunk_length += strlen(hunk->lines.data[i]) + 1;

    size_t file_header_size = snprintf(NULL, 0, file_diff_header, file->src, file->dest, file->src, file->dest);
    size_t hunk_header_size = snprintf(NULL, 0, hunk_diff_header, old_start, old_length, new_start, new_length);
    char *buffer = (char *) malloc(file_header_size + hunk_header_size + hunk_length + 1);

    char *p = buffer;
    p += snprintf(p, file_header_size + 1, file_diff_header, file->src, file->dest, file->src, file->dest);
    p += snprintf(p, hunk_header_size + 1, hunk_diff_header, old_start, old_length, new_start, new_length);

    for (size_t i = 0; i < hunk->lines.length; i++) {
        size_t len = strlen(hunk->lines.data[i]);
        memcpy(p, hunk->lines.data[i], len);
        p += len;
        *p++ = '\n';
    }
    *p = '\0';

    int status = git_apply(CMD("git", "apply", "--cached", "-"), buffer);
    if (status != 0) ERROR("Unable to stage the hunk.\n");

    free(buffer);
}

void git_unstage_hunk(File *file, Hunk *hunk) {}