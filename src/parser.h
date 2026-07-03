#ifndef PARSER_H
#define PARSER_H

#include "tokens.h"

int simple_command(const char *input);

Command *parse_line(char *line);
void free_command(Command *cmd);

#endif /* PARSER_H */