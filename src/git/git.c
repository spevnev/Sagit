#include <errno.h>
    if (patch == NULL) OUT_OF_MEMORY();
    if (patch_body == NULL) OUT_OF_MEMORY();
        if (patch == NULL) OUT_OF_MEMORY();
        if (patch == NULL) OUT_OF_MEMORY();
        if (patch == NULL) OUT_OF_MEMORY();
    if (stat(file_path, &file_info) == -1) ERROR("Unable to stat \"%s\": %s.\n", file_path, strerror(errno));
    if (fd == -1) ERROR("Unable to open \"%s\": %s.\n", file_path, strerror(errno));
    if (buffer == NULL) OUT_OF_MEMORY();
    if (read(fd, buffer, size) != (ssize_t) size) ERROR("Unable to read \"%s\": %s.\n", file_path, strerror((errno)));
        ERROR("Unable to stage the hunk. Failed patch written to \"%s\".\n", FAILED_PATCH_PATH);
        ERROR("Unable to unstage the hunk. Failed patch written to \"%s\".\n", FAILED_PATCH_PATH);
        ERROR("Unable to stage the range. Failed patch written to \"%s\".\n", FAILED_PATCH_PATH);
        ERROR("Unable to unstage the range. Failed patch written to \"%s\".\n", FAILED_PATCH_PATH);