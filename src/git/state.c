#include "state.h"
#include <stdlib.h>
#include "ctxt.h"
#include "error.h"
#include "vector.h"

void free_files(FileVec *files) {
    ASSERT(files != NULL);

    for (size_t i = 0; i < files->length; i++) {
        for (size_t j = 0; j < files->data[i].hunks.length; j++) {
            VECTOR_FREE(&files->data[i].hunks.data[j].lines);
        }
        VECTOR_FREE(&files->data[i].hunks);
    }
    VECTOR_FREE(files);
}

void free_state(State *state) {
    ASSERT(state != NULL);

    ctxt_free(&state->untracked_ctxt);

    free(state->unstaged.raw);
    free_files(&state->unstaged.files);

    free(state->staged.raw);
    free_files(&state->staged.files);
}
