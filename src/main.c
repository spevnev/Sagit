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

    if (git.untracked.files.length > 0) {
        printf("Unstaged files:\n");

        for (size_t i = 0; i < git.untracked.files.length; i++) {
            printf("created %s\n", git.untracked.files.data[i]);
        }

        printf("\n");
    }

    if (git.unstaged.files.length > 0) {
        printf("Unstaged changes:\n");

        for (size_t i = 0; i < git.unstaged.files.length; i++) {
            File file = git.unstaged.files.data[i];

            if (strcmp(file.dest, "/dev/null") == 0) {
                printf("deleted  %s\n", file.src + 2);
            } else if (strcmp(file.src + 2, file.dest + 2) == 0) {
                printf("renamed  %s -> %s\n", file.src + 2, file.dest + 2);
            } else {
                printf("modified %s\n", file.src + 2);
            }

            for (size_t i = 0; i < file.hunks.length; i++) {
                str_vec hunk = file.hunks.data[i];
                for (size_t j = 0; j < hunk.length; j++) {
                    printf("%s\n", hunk.data[j]);
                }
            }
        }

        printf("\n");
    }

    if (git.staged.files.length > 0) {
        printf("Staged changes:\n");

        for (size_t i = 0; i < git.staged.files.length; i++) {
            File file = git.staged.files.data[i];

            if (strcmp(file.dest, "/dev/null") == 0) {
                printf("deleted  %s\n", file.src + 2);
            } else if (strcmp(file.src + 2, file.dest + 2) == 0) {
                printf("renamed  %s -> %s\n", file.src + 2, file.dest + 2);
            } else {
                printf("modified %s\n", file.src + 2);
            }

            for (size_t i = 0; i < file.hunks.length; i++) {
                str_vec hunk = file.hunks.data[i];
                for (size_t j = 0; j < hunk.length; j++) {
                    printf("%s\n", hunk.data[j]);
                }
            }
        }

        printf("\n");
    }

    free(git.untracked.raw);
    VECTOR_FREE(&git.untracked.files);

    free(git.unstaged.raw);
    for (size_t i = 0; i < git.unstaged.files.length; i++) VECTOR_FREE(&git.unstaged.files.data[i].hunks);
    VECTOR_FREE(&git.unstaged.files);

    free(git.staged.raw);
    for (size_t i = 0; i < git.staged.files.length; i++) VECTOR_FREE(&git.staged.files.data[i].hunks);
    VECTOR_FREE(&git.staged.files);

    return EXIT_SUCCESS;
}
