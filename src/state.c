#include "state.h"
#include <stdlib.h>

void free_files(FileVec *vec) {
    for (size_t i = 0; i < vec->length; i++) {
        for (size_t j = 0; j < vec->data[i].hunks.length; j++) {
            VECTOR_FREE(&vec->data[i].hunks.data[j].lines);
        }
        VECTOR_FREE(&vec->data[i].hunks);
    }
    VECTOR_FREE(vec);
}