#ifndef EXEC_H
#define EXEC_H

#define CMD(...) ((char *const[]){__VA_ARGS__, NULL})

char *git_exec(int *status, char *const *args);

#endif  // EXEC_H