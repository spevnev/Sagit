#ifndef EXEC_H
#define EXEC_H

#define CMD(...) ((char *const[]){__VA_ARGS__, NULL})

// Runs git `args` and returns child's exit code.
int gexec(char *const *args);

// Runs git `args` and returns malloc()-ed merged stdout and stderr to `output`.
// If `exit_code` is not null, writes child's exit code to it.
char *gexecr(int *exit_code, char *const *args);

// Runs git `args` and writes `patch` to its standard input.
// Returns child's exit code.
int gexecw(char *const *args, const char *patch);

#endif  // EXEC_H