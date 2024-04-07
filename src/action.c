#include "action.h"
#include <string.h>
#include "git.h"
#include "state.h"

int section_action(void *_section, int ch) {
    Section *section = (Section *) _section;

    if (ch == ' ') {
        section->is_folded ^= 1;
        return AC_RERENDER;
    }

    return 0;
}

int untracked_file_action(void *_file_path, int ch) {
    char *file_path = (char *) _file_path;

    if (ch == 's') {
        git_stage_file(file_path);
        return AC_RERENDER | AC_UPDATE_STATE;
    }

    return 0;
}

int unstaged_file_action(void *_file, int ch) {
    File *file = (File *) _file;

    if (ch == ' ') {
        file->is_folded ^= 1;
        return AC_RERENDER;
    } else if (ch == 's') {
        git_stage_file(file->src + 2);
        return AC_RERENDER | AC_UPDATE_STATE;
    }

    return 0;
}

int staged_file_action(void *_file, int ch) {
    File *file = (File *) _file;

    if (ch == ' ') {
        file->is_folded ^= 1;
        return AC_RERENDER;
    } else if (ch == 'u') {
        git_unstage_file(file->dest + 2);
        return AC_RERENDER | AC_UPDATE_STATE;
    }

    return 0;
}

int unstaged_hunk_action(void *_args, int ch) {
    HunkArgs *args = (HunkArgs *) _args;

    if (ch == ' ') {
        args->hunk->is_folded ^= 1;
        return AC_RERENDER;
    } else if (ch == 's') {
        git_stage_hunk(args->file, args->hunk);
        return AC_RERENDER | AC_UPDATE_STATE;
    }

    return 0;
}

int staged_hunk_action(void *_args, int ch) {
    HunkArgs *args = (HunkArgs *) _args;

    if (ch == ' ') {
        args->hunk->is_folded ^= 1;
        return AC_RERENDER;
    } else if (ch == 'u') {
        git_unstage_hunk(args->file, args->hunk);
        return AC_RERENDER | AC_UPDATE_STATE;
    }

    return 0;
}

int unstaged_line_action(void *_line, int ch) {
    char *line = (char *) _line;

    if (ch == ' ') {
        return AC_TOGGLE_SELECTION;
    } else if (ch == 's') {
        // git_stage_line(file, line);
        return AC_RERENDER | AC_UPDATE_STATE;
    }

    return 0;
}

int staged_line_action(void *_line, int ch) {
    char *line = (char *) _line;

    if (ch == ' ') {
        return AC_TOGGLE_SELECTION;
    } else if (ch == 'u') {
        // git_unstage_line(file, line);
        return AC_RERENDER | AC_UPDATE_STATE;
    }

    return 0;
}