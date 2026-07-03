#ifndef BUILTINS_H
#define BUILTINS_H

#include "tokens.h"

int shell_cd(Command *cmd);
int shell_exit(Command *cmd);
int execute_builtin(Command *cmd);

#endif /* BUILTINS_H */