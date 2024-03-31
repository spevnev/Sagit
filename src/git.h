#ifndef GIT_H
#define GIT_H

typedef struct {
    char *from;
    char *to;
    char **hunks;
} File;

typedef struct {
    char **untracked;
    File *unstaged;
    File *staged;
} GitState;

int is_git_initialized(void);
int get_git_state(GitState *git);

#endif  // GIT_H