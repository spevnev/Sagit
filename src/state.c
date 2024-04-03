#include "state.h"
#include <stdlib.h>
#include "vector.h"

static void free_files(FileVec *vec) {
    for (size_t i = 0; i < vec->length; i++) {
        for (size_t j = 0; j < vec->data[i].hunks.length; j++) {
            VECTOR_FREE(&vec->data[i].hunks.data[j].lines);
        }
        VECTOR_FREE(&vec->data[i].hunks);
    }
    VECTOR_FREE(vec);
}

void free_state(State *state) {
    free(state->untracked.raw);
    VECTOR_FREE(&state->untracked.files);

    free(state->unstaged.raw);
    free_files(&state->unstaged.files);

    free(state->staged.raw);
    free_files(&state->staged.files);
}