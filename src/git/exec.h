#ifndef EXEC_H
#define EXEC_H

#define CMD(...) ((char *const[]){__VA_ARGS__, NULL})

// Runs git `args` and returns child's exit code.
int gexec(char *const *args);

// Runs git `args` and returns malloc()-ed stdout.
char *gexecr(char *const *args);

// Runs git `args` and writes `buffer` to its standard input.
// Returns child's exit code.
int gexecw(char *const *args, const char *buffer);

#endif  // EXEC_H