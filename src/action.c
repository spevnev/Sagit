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

int file_action(void *_file, int ch) {
    File *file = (File *) _file;

    if (ch == ' ') {
        file->is_folded ^= 1;
        return AC_RERENDER;
    } else if (ch == 'u') {
        // TODO: check that it is staged
        git_unstage_file(file->dest + 2);
        return AC_RERENDER | AC_UPDATE_STATE;
    } else if (ch == 's') {
        // TODO: check that it is unstaged/tracked
        git_stage_file(file);
        return AC_RERENDER | AC_UPDATE_STATE;
    }

    return 0;
}

int hunk_action(void *_hunk, int ch) {
    Hunk *hunk = (Hunk *) _hunk;

    if (ch == ' ') {
        hunk->is_folded ^= 1;
        return AC_RERENDER;
    }

    return 0;
}

int line_action(void *_line, int ch) {
    char *line = (char *) _line;

    return 0;
}