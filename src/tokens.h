#ifndef TOKENS_H
#define TOKENS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum
{
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_REDIRECT_IN,
    TOKEN_REDIRECT_OUT,
    TOKEN_APPEND,
    TOKEN_BACKGROUND,
    TOKEN_SEMICOLON,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_EOF
} TokenType;

typedef enum
{
    QUOTE_NONE,
    QUOTE_SINGLE,
    QUOTE_DOUBLE,
    QUOTE_MIXED
} QuoteType;


typedef struct
{
    TokenType type;

    char *text; // The actual text of the token (e.g., command name, argument, operator)

    QuoteType quote; // Indicates the type of quotes surrounding the token, if any

    bool single_quoted; // Indicates if the token was enclosed in single quotes
    bool double_quoted; // Indicates if the token was enclosed in double quotes

    bool escaped; // Indicates if the token was escaped with a backslash

    bool contains_variable; // Indicates if the token contains a variable (e.g., $VAR)
    bool contains_command_substitution; // Indicates if the token contains command substitution (e.g., $(command))
    bool contains_arithmetic; // Indicates if the token contains arithmetic expansion (e.g., $((1 + 2)))

    bool tilde_expand; // Indicates if the token should undergo tilde expansion (e.g., ~/)
    bool word_split; // Indicates if the token is eligible for word splitting (e.g., unquoted tokens)
    bool pathname_expand; // Indicates if the token should undergo pathname expansion (globbing)
} Token;

typedef struct
{
    Token *argv; // Array of tokens representing the command and its arguments
    size_t argc; // Number of tokens in argv
} Command;

void token_init(Token *token);
void token_add_text(Token *token, const char *text, size_t length);
void token_print(const Token *token);

#endif /* TOKENS_H */