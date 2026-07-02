#ifndef PARSER_H
#define PARSER_H

int simple_command(const char *input);

char **parse_line(char *line);
void free_tokens(char **tokens);

#endif /* PARSER_H */