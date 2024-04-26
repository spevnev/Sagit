#ifndef SORT_H
#define SORT_H

#include "git/state.h"
#include "vector.h"

// Returns a vector of indexes which point to files in the original vector
size_vec sort_files(const FileVec *files);

#endif  // SORT_H