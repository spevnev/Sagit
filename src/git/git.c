#define FAILED_PATCH_PATH ".failed_patch"
#define DUMP_PATCH(patch)                                           \
    do {                                                            \
        int fd = open(FAILED_PATCH_PATH, O_WRONLY | O_CREAT, 0755); \
        write(fd, patch, strlen(patch));                            \
        ERROR("Unable to stage the hunk. Failed patch written to %s.\n", FAILED_PATCH_PATH);
        ERROR("Unable to unstage the hunk. Failed patch written to %s.\n", FAILED_PATCH_PATH);
        ERROR("Unable to stage the range. Failed patch written to %s.\n", FAILED_PATCH_PATH);
        ERROR("Unable to unstage the range. Failed patch written to %s.\n", FAILED_PATCH_PATH);