#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "git.h"

int main(int argc, char **argv) {
    if (argc > 1) {
        fprintf(stderr, "usage: %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!is_git_initialized()) ERROR("Git is not initialized in the current directory.\n");

    GitState git = {0};
    if (get_git_state(&git)) ERROR("Unable to get git state.\n");

    printf("Unstaged files:\n");
    for (size_t i = 0; i < git.untracked.files.length; i++) {
        printf("created %s\n", git.untracked.files.data[i]);
    }

    printf("\nUnstaged changes:\n");
    for (size_t i = 0; i < git.unstaged.files.length; i++) {
        File file = git.unstaged.files.data[i];

        if (strcmp(file.dest, "/dev/null") == 0) {
            printf("deleted  %s\n", file.src + 2);
        } else if (strcmp(file.src + 2, file.dest + 2) == 0) {
            printf("renamed  %s -> %s\n", file.src + 2, file.dest + 2);
        } else {
            printf("modified %s\n", file.src + 2);
        }
    }

    printf("\nStaged changes:\n");
    for (size_t i = 0; i < git.staged.files.length; i++) {
        File file = git.staged.files.data[i];

        if (strcmp(file.dest, "/dev/null") == 0) {
            printf("deleted  %s\n", file.src + 2);
        } else if (strcmp(file.src + 2, file.dest + 2) == 0) {
            printf("renamed  %s -> %s\n", file.src + 2, file.dest + 2);
        } else {
            printf("modified %s\n", file.src + 2);
        }
    }

    free(git.untracked.raw);
    VECTOR_FREE(&git.untracked.files);
    free(git.unstaged.raw);
    VECTOR_FREE(&git.unstaged.files);
    free(git.staged.raw);
    VECTOR_FREE(&git.staged.files);

    return EXIT_SUCCESS;
}
