#ifndef PATCH
#define PATCH

#include <fcntl.h>
#include <stdlib.h>
#include "git/state.h"

#define FAILED_PATCH_PATH ".failed_patch"

#define DUMP_PATCH(patch)                                                     \
    do {                                                                      \
        int fd = open(FAILED_PATCH_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0755); \
        size_t len = strlen(patch);                                           \
        size_t bytes = write(fd, patch, len);                                 \
        (void) bytes;                                                         \
        ASSERT(bytes == len);                                                 \
        close(fd);                                                            \
    } while (0);

char *create_patch_from_hunk(const File *file, const Hunk *hunk);
// Returns NULL in case patch doesn't contain any changes or doesn't need to be applied
char *create_patch_from_range(const File *file, const Hunk *hunk, size_t range_start, size_t range_end, bool reverse);

#endif  //  PATCH
