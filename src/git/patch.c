static const char *file_header_fmt = "diff --git %s %s\n--- a/%s\n+++ b/%s\n";
static const char *new_file_header_fmt = "diff --git %s %s\nnew file mode %o\n--- /dev/null\n+++ b/%s\n";
static const char *del_file_header_fmt = "diff --git %s %s\ndeleted file mode %o\n--- a/%s\n+++ /dev/null\n";
    char *dst_path = (char *) ctxt_alloc(ctxt, length + 1);
    memcpy(dst_path, file_path, length);
    dst_path[length] = '\0';
        git_unstage_file(file->dst);
        if (stat(file->dst, &file_info) == -1) ERROR("Unable to stat \"%s\": %s.\n", file->dst, strerror(errno));