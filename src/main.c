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

    // GitState git;
    // if (get_git_state(&git)) ERROR("Unable to get git state.\n");

    // TODO: print git state

    return EXIT_SUCCESS;
}
