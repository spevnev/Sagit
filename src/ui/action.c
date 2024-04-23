#include "ui/action.h"
#include "git/git.h"
#include "git/state.h"
#include "utils/error.h"

int section_action(void *_section, const ActionArgs *args) {
    Section *section = (Section *) _section;
    ASSERT(section != NULL && args != NULL);

    if (args->ch == ' ') {
        section->is_folded = !section->is_folded;
        return AC_RERENDER;
    }

    return 0;
}

int untracked_file_action(void *_file_path, const ActionArgs *args) {
    char *file_path = (char *) _file_path;
    ASSERT(file_path != NULL && args != NULL);

    if (args->ch == 's') {
        git_stage_file(file_path);
        return AC_UPDATE_STATE;
    }

    return 0;
}

int unstaged_file_action(void *_file, const ActionArgs *args) {
    File *file = (File *) _file;
    ASSERT(file != NULL && args != NULL);

    if (args->ch == ' ') {
        file->is_folded = !file->is_folded;
        return AC_RERENDER;
    } else if (args->ch == 's') {
        if (file->change_type == FC_MODIFIED || file->change_type == FC_DELETED) {
            git_stage_file(file->src + 2);
        } else if (file->change_type == FC_CREATED) {
            git_stage_file(file->dst + 2);
        } else if (file->change_type == FC_RENAMED) {
            git_stage_file(file->src + 2);
            git_stage_file(file->dst + 2);
        } else UNREACHABLE();

        return AC_UPDATE_STATE;
    }

    return 0;
}

int staged_file_action(void *_file, const ActionArgs *args) {
    File *file = (File *) _file;
    ASSERT(file != NULL && args != NULL);

    if (args->ch == ' ') {
        file->is_folded = !file->is_folded;
        return AC_RERENDER;
    } else if (args->ch == 'u') {
        if (file->change_type == FC_MODIFIED || file->change_type == FC_DELETED) {
            git_unstage_file(file->src + 2);
        } else if (file->change_type == FC_CREATED) {
            git_unstage_file(file->dst + 2);
        } else if (file->change_type == FC_RENAMED) {
            git_unstage_file(file->src + 2);
            git_unstage_file(file->dst + 2);
        } else UNREACHABLE();

        return AC_UPDATE_STATE;
    }

    return 0;
}

int unstaged_hunk_action(void *_hunk_args, const ActionArgs *args) {
    HunkArgs *hunk_args = (HunkArgs *) _hunk_args;
    ASSERT(hunk_args != NULL && args != NULL);
    Hunk *hunk = hunk_args->hunk;

    if (args->ch == ' ') {
        hunk->is_folded = !hunk->is_folded;
        return AC_RERENDER;
    } else if (args->ch == 's') {
        git_stage_hunk(hunk_args->file, hunk);
        return AC_UPDATE_STATE;
    }

    return 0;
}

int staged_hunk_action(void *_hunk_args, const ActionArgs *args) {
    HunkArgs *hunk_args = (HunkArgs *) _hunk_args;
    ASSERT(hunk_args != NULL && args != NULL);
    Hunk *hunk = hunk_args->hunk;

    if (args->ch == ' ') {
        hunk->is_folded = !hunk->is_folded;
        return AC_RERENDER;
    } else if (args->ch == 'u') {
        git_unstage_hunk(hunk_args->file, hunk);
        return AC_UPDATE_STATE;
    }

    return 0;
}

int unstaged_line_action(void *_line_args, const ActionArgs *args) {
    LineArgs *line_args = (LineArgs *) _line_args;
    ASSERT(line_args != NULL && args != NULL);

    if (args->ch == ' ') {
        return AC_TOGGLE_SELECTION;
    } else if (args->ch == 's') {
        if (args->range_start == -1) {
            if (line_args->hunk->lines.data[line_args->line][0] == ' ') return 0;
            git_stage_range(line_args->file, line_args->hunk, line_args->line, line_args->line);
            return AC_UPDATE_STATE;
        } else {
            git_stage_range(line_args->file, line_args->hunk, args->range_start - line_args->hunk_y, args->range_end - line_args->hunk_y);
            return AC_TOGGLE_SELECTION | AC_UPDATE_STATE;
        }
    }

    return 0;
}

int staged_line_action(void *_line_args, const ActionArgs *args) {
    LineArgs *line_args = (LineArgs *) _line_args;
    ASSERT(line_args != NULL && args != NULL);

    if (args->ch == ' ') {
        return AC_TOGGLE_SELECTION;
    } else if (args->ch == 'u') {
        if (args->range_start == -1) {
            if (line_args->hunk->lines.data[line_args->line][0] == ' ') return 0;
            git_unstage_range(line_args->file, line_args->hunk, line_args->line, line_args->line);
            return AC_UPDATE_STATE;
        } else {
            git_unstage_range(line_args->file, line_args->hunk, args->range_start - line_args->hunk_y, args->range_end - line_args->hunk_y);
            return AC_TOGGLE_SELECTION | AC_UPDATE_STATE;
        }
    }

    return 0;
}