#ifndef PARSER_H
#define PARSER_H

#include "tokens.h"

#define INITIAL_TOKEN_CAPACITY 64 // Initial capacity for the token array

int simple_command(const char *input);

Command *parse_line(char *line);
void free_command(Command *cmd);

#endif /* PARSER_H */