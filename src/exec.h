#ifndef EXEC_H
#define EXEC_H

#define CMD(...) ((char *const[]){__VA_ARGS__, NULL})

// Runs git `args` and returns child's exit code.
int git_exec_without_output(char *const *args);

// Runs git `args` nad returns malloc()-ed merged stdout and stderr to `output`.
// If `exit_code` is not null, writes child's exit code to it.
char *git_exec(int *exit_code, char *const *args);

// Runs git `args`, writes `patch` to its standard input.
// Returns child's exit code.
int git_apply(char *const *args, const char *patch);

#endif  // EXEC_H