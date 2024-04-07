#ifndef EXEC_H
#define EXEC_H

#include "vector.h"

#define CMD(...) ((char *const[]){__VA_ARGS__, NULL})

// If `status` is not null, writes child exit code, otherwise if child fails it exit(1)-s.
// Returns malloc()-ed stdout and stderr output (must be freed by the caller).
char *git_exec(int *status, char *const *args);

// Runs git `args` and writes `patch` to standard input.
// Returns child's exit code.
int git_apply(char *const *args, const char *patch);
int git_apply_array(char *const *args, const str_vec *patches);

#endif  // EXEC_H