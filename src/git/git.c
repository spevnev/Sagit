#define PATCH_PATH ".failed_patch"

#define DUMP_PATCH(patch)                                    \
    do {                                                     \
        int fd = open(PATCH_PATH, O_WRONLY | O_CREAT, 0755); \
        write(fd, patch, strlen(patch));                     \
    } while (0);

        file.is_folded = true;
        file.dst = lines.data[i++] + 4;
        if (strcmp(file.src + 2, file.dst + 2) == 0) file.change_type = FC_MODIFIED;
        else if (strcmp(file.dst, "/dev/null") == 0) file.change_type = FC_DELETED;
    ASSERT(old_files != NULL && new_files != NULL);

typedef struct {
    int start;
    int old_length;
    int new_length;
} HunkHeader;

HunkHeader parse_hunk_header(const char *raw) {
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

    ASSERT(file != NULL && hunk != NULL);

    HunkHeader header = parse_hunk_header(hunk->header);
    size_t file_header_size = snprintf(NULL, 0, file_header_fmt, file->src, file->dst, file->src, file->dst);
    size_t hunk_header_size = snprintf(NULL, 0, hunk_header_fmt, header.start, header.old_length, header.start, header.new_length);
    if (patch == NULL) ERROR("Process is out of memory.\n");
    ptr += snprintf(ptr, file_header_size + 1, file_header_fmt, file->src, file->dst, file->src, file->dst);
    ptr += snprintf(ptr, hunk_header_size + 1, hunk_header_fmt, header.start, header.old_length, header.start, header.new_length);
static char *create_patch_from_range(const File *file, const Hunk *hunk, size_t range_start, size_t range_end, bool reverse) {
    ASSERT(file != NULL && hunk != NULL);

    HunkHeader header = parse_hunk_header(hunk->header);
    if (patch_body == NULL) ERROR("Process is out of memory.\n");
    bool has_changes = false;
                header.new_length--;
                header.old_length--;
                header.new_length++;
                header.old_length++;
            if (str[0] == '-' || str[0] == '+') has_changes = true;
    if (header.new_length == 0) {
        git_unstage_file(file->dst + 2);
    if (!has_changes) {
    size_t hunk_header_size = snprintf(NULL, 0, hunk_header_fmt, header.start, header.old_length, header.start, header.new_length);
        size_t file_header_size = snprintf(NULL, 0, file_header_fmt, file->src, file->dst, file->src, file->dst);
        if (patch == NULL) ERROR("Process is out of memory.\n");
        ptr += snprintf(ptr, file_header_size + 1, file_header_fmt, file->src, file->dst, file->src, file->dst);
    } else if (reverse) {  // Partial unstaging of untracked file requires src == dst
        size_t file_header_size = snprintf(NULL, 0, file_header_fmt, file->dst, file->dst, file->dst, file->dst);
        if (patch == NULL) ERROR("Process is out of memory.\n");
        ptr += snprintf(ptr, file_header_size + 1, file_header_fmt, file->dst, file->dst, file->dst, file->dst);
        if (stat(file->dst + 2, &file_info) == -1) ERROR("Unable to stat \"%s\".\n", file->dst + 2);
        size_t file_header_size = snprintf(NULL, 0, new_file_header_fmt, file->dst, file->dst, file_info.st_mode, file->dst);
        if (patch == NULL) ERROR("Process is out of memory.\n");
        ptr += snprintf(ptr, file_header_size + 1, new_file_header_fmt, file->dst, file->dst, file_info.st_mode, file->dst);
    ptr += snprintf(ptr, hunk_header_size + 1, hunk_header_fmt, header.start, header.old_length, header.start, header.new_length);
    ASSERT(ctxt != NULL && file_path != NULL);

    if (buffer == NULL) ERROR("Process is out of memory.\n");
bool is_git_initialized(void) { return gexec(CMD("git", "status")) == 0; }
bool is_state_empty(State *state) {
    ASSERT(state != NULL);
    return state->unstaged.files.length == 0 && state->staged.files.length == 0;
bool is_ignored(char *file_path) {
    ASSERT(file_path != NULL);
    return gexec(CMD("git", "check-ignore", file_path)) == 0;
    state->unstaged.raw = gexecr(CMD_UNSTAGED);
    // TODO: create a separate function
    char *raw_file_paths = gexecr(CMD_UNTRACKED);
    state->staged.raw = gexecr(CMD_STAGED);
    char *unstaged_raw = gexecr(CMD_UNSTAGED);
    char *raw_file_paths = gexecr(CMD_UNTRACKED);
    char *staged_raw = gexecr(CMD_STAGED);
void git_stage_file(const char *file_path) {
    ASSERT(file_path != NULL);
    if (gexec(CMD("git", "add", (char *) file_path)) != 0) ERROR("Unable to stage file.\n");
}
void git_unstage_file(const char *file_path) {
    ASSERT(file_path != NULL);
    if (gexec(CMD("git", "restore", "--staged", (char *) file_path)) != 0) ERROR("Unable to unstage file.\n");
}
    ASSERT(file != NULL && hunk != NULL);

    if (gexecw(CMD_APPLY, patch) != 0) {
        DUMP_PATCH(patch);
        ERROR("Unable to stage the hunk. Failed patch written to %s.\n", PATCH_PATH);
    }
    ASSERT(file != NULL && hunk != NULL);

    if (gexecw(CMD_APPLY_REVERSE, patch) != 0) {
        DUMP_PATCH(patch);
        ERROR("Unable to unstage the hunk. Failed patch written to %s.\n", PATCH_PATH);
    }
    ASSERT(file != NULL && hunk != NULL);

    if (gexecw(CMD_APPLY, patch) != 0) {
        DUMP_PATCH(patch);
        ERROR("Unable to stage the range. Failed patch written to %s.\n", PATCH_PATH);
    }
    ASSERT(file != NULL && hunk != NULL);

    if (gexecw(CMD_APPLY_REVERSE, patch) != 0) {
        DUMP_PATCH(patch);
        ERROR("Unable to unstage the range. Failed patch written to %s.\n", PATCH_PATH);
    }