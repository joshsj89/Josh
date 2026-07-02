#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    TOKEN_WORD,
    TOKEN_EOF,
    TOKEN_INVALID
} TokenType;

typedef struct Token
{
    TokenType type;
    char *value;
} Token;

// Struct to hold the parsed simple command
typedef struct SimpleCommand
{
    char *command;
    char **args;
    int argc;
} SimpleCommand;

// Global or context-based variables for parsing state
extern Token *tokens;
extern int current_token_idx;

// Helper to peek at the current token
Token peek_token()
{
    return tokens[current_token_idx];
}

// Helper to consume and advance to the next token
Token consume_token()
{
    return tokens[current_token_idx++];
}

// Forward declaration
SimpleCommand *parse_simple_command();

// Parses a list of arguments (0 or more)
char **parse_argument_list(int *argc)
{
    int capacity = 10;
    char **arguments = malloc(sizeof(char *) * capacity);
    int count = 0;

    while (peek_token().type == TOKEN_WORD)
    {
        if (count >= capacity - 1)
        {
            capacity *= 2;
            arguments = realloc(arguments, sizeof(char *) * capacity);
        }
        arguments[count++] = strdup(consume_token().value);
    }

    arguments[count] = NULL; // Null-terminate for execvp()
    *argc = count;
    return arguments;
}

// Parses a simple command: One Word followed by Argument List
SimpleCommand *parse_simple_command()
{
    Token cmd_token = peek_token();

    // A simple command must start with a command name (word)
    if (cmd_token.type != TOKEN_WORD)
    {
        fprintf(stderr, "Syntax Error: Expected command name, got token type %d\n", cmd_token.type);
        return NULL;
    }

    // Allocate our command struct
    SimpleCommand *sc = malloc(sizeof(SimpleCommand));
    sc->command = strdup(consume_token().value);

    // Recursively parse the argument list
    sc->args = parse_argument_list(&sc->argc);

    return sc;
}
