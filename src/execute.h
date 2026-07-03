#ifndef EXECUTE_H
#define EXECUTE_H

#include "tokens.h"

char **command_to_argv(const Command *cmd);
void execute_command(Command *cmd);

#endif /* EXECUTE_H */