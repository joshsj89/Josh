#ifndef BUILTINS_H
#define BUILTINS_H

int shell_cd(char **args);
int shell_exit(char **args);
int execute_builtin(char **args);

#endif /* BUILTINS_H */