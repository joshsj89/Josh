#ifndef TOKENS_H
#define TOKENS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum TokenType
{
    TOKEN_WORD,
    
    TOKEN_BACKGROUND = '&',
    TOKEN_LPAREN = '(',
    TOKEN_RPAREN = ')',
    TOKEN_SEMICOLON = ';',
    TOKEN_REDIRECT_IN = '<',
    TOKEN_REDIRECT_OUT = '>',
    TOKEN_PIPE = '|',

    TOKEN_AND,
    TOKEN_OR,
    TOKEN_APPEND,
    TOKEN_EOF
} TokenType;

typedef enum SegmentType
{
    SEG_LITERAL,
    SEG_VARIABLE, // Indicates if the segment contains a variable (e.g., $VAR)
    SEG_COMMAND, // Indicates if the segment contains a command substitution (e.g., $(command))
    SEG_ARITHMETIC // Indicates if the segment contains an arithmetic expansion (e.g., $((1 + 2)))
} SegmentType;

typedef enum QuoteType
{
    QUOTE_NONE,
    QUOTE_SINGLE, // Indicates if the segment is enclosed in single quotes
    QUOTE_DOUBLE, // Indicates if the segment is enclosed in double quotes
} QuoteType;

typedef struct
{
    SegmentType type; // Type of the segment (literal, variable, command substitution, arithmetic)
    char *text;       // The actual text of the segment
    QuoteType quote; // Indicates the type of quotes surrounding the segment, if any

    bool escaped; // Indicates if the segment was escaped with a backslash

    bool allow_word_split; // Indicates if the segment is eligible for word splitting (e.g., unquoted segments)
    bool pathname_expand; // Indicates if the segment should undergo pathname expansion (globbing)
} Segment;

typedef struct
{
    TokenType type;
    Segment *segments; // Array of segments that make up the token
    size_t segment_count; // Number of segments in the token

    char *full_text; // The full text of the token (concatenation of all segments)

    bool tilde_expand; // Indicates if the token should undergo tilde expansion (e.g., ~/)
} Token;

typedef struct
{
    Token *argv; // Array of tokens representing the command and its arguments
    size_t argc; // Number of tokens in argv
} Command;

void token_init(Token *token);
void token_add_segment(Token *token, SegmentType type, const char *text, size_t length, QuoteType quote);
void token_print(const Token *token);

#endif /* TOKENS_H */