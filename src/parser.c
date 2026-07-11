/*
 *      parser.c
 *
 *      This file contains the implementation of the command parser for the shell.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "tokens.h"

/*
 * Function: is_operator_char
 * --------------------------
 * Checks if a character is an operator character.
 *
 * Parameters:
 *   c - The character to check.
 *
 * Returns:
 *   true if the character is an operator character, false otherwise.
 */
static bool is_operator_char(char c)
{
    switch (c)
    {
        case '&':
        case '(':
        case ')':
        case ';':
        case '<':
        case '>':
        case '|':
            return true;
        default:
            return false;
    }
}

/*
 * Function: operator_type
 * -----------------------
 * Determines the type of an operator based on its character representation.
 *
 * Parameters:
 *   p - A pointer to the operator character.
 *   length - A pointer to the length of the operator.
 *
 * Returns:
 *   The type of the operator.
 */
static TokenType operator_type(const char *p, size_t *length)
{
    switch (*p)
    {
        case '&':
            if (p[1] == '&')
            {
                *length = 2;
                return TOKEN_AND;
            }
            *length = 1;
            return TOKEN_BACKGROUND;
        case '|':
            if (p[1] == '|')
            {
                *length = 2;
                return TOKEN_OR;
            }
            *length = 1;
            return TOKEN_PIPE;
        case '>':
            if (p[1] == '>')
            {
                *length = 2;
                return TOKEN_APPEND;
            }
            *length = 1;
            return TOKEN_REDIRECT_OUT;
        case '(':
            *length = 1;
            return TOKEN_LPAREN;
        case ')':
            *length = 1;
            return TOKEN_RPAREN;
        case ';':
            *length = 1;
            return TOKEN_SEMICOLON;
        case '<':
            *length = 1;
            return TOKEN_REDIRECT_IN;
        default:
            *length = 1;
            return TOKEN_WORD; // Default to TOKEN_WORD for non-operator characters
    }
}

/*
 * Function: token_init
 * --------------------
 * Initializes a Token structure with default values.
 *
 * Parameters:
 *   token - A pointer to the Token structure to initialize.
 * 
 */
void token_init(Token *token)
{
    token->type = TOKEN_WORD;
    token->segments = NULL;
    token->segment_count = 0;

    token->full_text = NULL;

    token->tilde_expand = false;
}

/*
 * Function: token_add_segment
 * ----------------------------
 * Adds a new segment to the token's segments array.
 *
 * Parameters:
 *   token - A pointer to the Token structure.
 *   type - The type of the segment.
 *   text - The text of the segment.
 *   length - The length of the segment text.
 *   quote - The type of quotes surrounding the segment, if any.
 * 
 */
void token_add_segment(Token *token, SegmentType type, const char *text, size_t length, QuoteType quote)
{
    token->segments = realloc(token->segments, (token->segment_count + 1) * sizeof(Segment));
    if (!token->segments)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Add the new segment to the token's segments array
    Segment *seg = &token->segments[token->segment_count++]; // Get a pointer to the new segment

    // Initialize the new segment
    seg->type = type;
    seg->text = strndup(text, length);
    seg->quote = quote;
    seg->escaped = false;
    seg->allow_word_split = quote == QUOTE_NONE && type != SEG_LITERAL; // Allow word splitting only if the segment is not quoted
    seg->pathname_expand = false;
}

/*
 * Function: flush_literal
 * ------------------------
 * Adds a literal segment to the token's segments array.
 *
 * Parameters:
 *   token - A pointer to the Token structure.
 *   start - A pointer to the start of the literal text.
 *   end - A pointer to the end of the literal text.
 *   quote - The type of quotes surrounding the segment, if any.
 */
static void flush_literal(Token *token, const char *start, const char *end, QuoteType quote)
{
    if (end > start) // Only add a segment if there is text to add
        token_add_segment(token, SEG_LITERAL, start, end - start, quote);
}

/*
 * Function: token_print
 * ----------------------
 * Prints the details of a Token structure.
 *
 * Parameters:
 *   token - A pointer to the Token structure to print.
 */
void token_print(const Token *token)
{
    printf("Token Type: %d\n", token->type);
    printf("Segment Count: %zu\n", token->segment_count);
    for (size_t i = 0; i < token->segment_count; i++)
        printf("Segment %zu: %s\n", i, token->segments[i].text);
}

/*
 * Function: expression
 * --------------------
 * Parses a Bash expression, tracks parenthesis depth
 * and verifies syntax correctness.
 *
 * Parameters:
 *   input - The input Bash expression string.
 *
 * Returns:
 *   The length of the expression if parentheses are balanced, -1 otherwise.
 *
 * Note:
 *   This function assumes the input string begins at the first character of the expression and does not skip leading whitespace.
 *   The length returned is the number of characters processed in the expression.
 *   input + length will point to the character after the expression, which is useful for further processing of the input string.
 */
int expression(const char *input)
{
    int depth = 0; // Track the depth of nested parentheses
    int length = 0; // Track the length of the expression

    while (*input)
    {
        if (*input == '(')
            depth++;
        else if (*input == ')')
        {
            depth--;

            // If depth goes below 0, we have a closing parenthesis
            // before an opening one (syntax error)
            if (depth < 0)
            {
                fprintf(stderr, "Syntax Error: Unbalanced closing parenthesis");
                return -1; // Return -1 to indicate an error
            }
        }

        length++; // Increment the length of the expression
        input++;
        
        if (depth == 0 && *input == ')') // If we are at the top level and encounter a closing parenthesis, stop
            break;
    }

    return length;
}

/*
 * Function: command_substitution
 * -------------------------------
 * Parses a command substitution, tracks parenthesis depth
 * and verifies syntax correctness.
 *
 * Parameters:
 *   input - The input string starting with the command substitution.
 * 
 * Returns:
 *   The length of the command substitution if parentheses are balanced, -1 otherwise.
 * 
 * Note:
 *   This function assumes the input string begins at the first character of the command substitution and does not skip leading whitespace.
 *   The length returned is the number of characters processed in the command substitution. 
 *   input + length will point to the character after the command substitution, which is useful for further processing of the input string.
 */
int command_substitution(const char *input)
{
    int length = 0; // Track the length of the command substitution
    bool in_single = false; // Track if we are inside single quotes
    bool in_double = false; // Track if we are inside double quotes

    while (*input)
    {
        if (*input == '\'' && !in_double)
            in_single = !in_single; // Toggle single quote state
        else if (*input == '\"' && !in_single)
            in_double = !in_double; // Toggle double quote state
        else if (*input == '\\') // Handle escape character
        {
            input++; // Skip the next character (escaped character will be treated as a literal)
            length++; // Count the escaped character
        }
        else if (!in_single && !in_double)
        {
            if (*input == '$' && *(input + 1) == '(' && *(input + 2) == '(')
            {
                input += 3; // Move past the '$' and the two '(' characters
                length += 3; // Count the '$' and the two '(' characters
                
                int expr_length = expression(input); // Get the length of the expression inside $((
                if (expr_length == -1) // Check for syntax error in expression
                    return -1; // Return -1 to indicate an error

                input += expr_length; // Skip to the end of the expression inside $(())
                length += expr_length; // Count the characters in the expression

                if (*input != ')' || *(input + 1) != ')') // Ensure we have a closing parentheses
                {
                    fprintf(stderr, "Error: Unmatched opening parenthesis in input");
                    return -1; // Return -1 to indicate an error
                }

                input += 2; // Move past the closing parentheses
                length += 2; // Count the closing parentheses

                continue;
            }
            
            if (*input == '$' && *(input + 1) == '(')
            {
                input += 2; // Move past the '$' and the '(' characters
                length += 2; // Count the '$' and the '(' characters
                
                int cmd_length = command_substitution(input); // Get the length of the command substitution inside $()
                if (cmd_length == -1) // Check for syntax error in command substitution
                    return -1; // Return -1 to indicate an error

                input += cmd_length; // Skip to the end of the command substitution inside $()
                length += cmd_length; // Count the characters in the command substitution

                if (*input != ')') // Ensure we have a closing parenthesis
                {
                    fprintf(stderr, "Error: Unmatched opening parenthesis in input");
                    return -1; // Return -1 to indicate an error
                }

                input += 1; // Move past the closing parenthesis
                length += 1; // Count the closing parenthesis

                continue;
            }
            
            if (*input == ')')
                break;
        }

        length++; // Increment the length of the command substitution
        input++;
    }

    if (in_double)
    {
        fprintf(stderr, "Error: Unmatched double quotes in input");
        return -1; // Return -1 to indicate an error
    }

    if (in_single)
    {
        fprintf(stderr, "Error: Unmatched single quotes in input");
        return -1; // Return -1 to indicate an error
    }

    return length; // Return the length of the command substitution
}

/*
 * Function: tokenize_line
 * ------------------------
 * Tokenizes a line of input into individual tokens.
 *
 * This function takes a line of input and splits it into individual tokens.
 * It handles quotes, escape characters, and special characters like variables and command substitutions.
 * It updates the provided Token structure with the parsed token information.
 *
 * Parameters:
 *   line - A pointer to the input string to be tokenized.
 *   token - A pointer to a Token structure to store the parsed token.
 * 
 * Note:
 *  This function maintains a static pointer to the start of the current token, allowing it to continue parsing 
 *  from where it left off in subsequent calls (similar to strtok()). For subsequent calls, pass NULL as the line
 *  parameter to continue tokenizing the same input string. token's text will be NULL when there are no more tokens
 *  to parse.
 * 
 */
static void tokenize_line(char *line, Token *token)
{
    token_init(token); // Initialize the token structure
    QuoteType quote_state = QUOTE_NONE; // Track the current quote state

    static char *token_start = NULL; // Pointer to the start of the current token

    // Continue from the last position if line is NULL
    if (line == NULL)
        line = token_start;

     // If the line is empty, reset token_start and return NULL
    if (*line == '\0')
    {
        token_start = NULL;
        return;
    }

    // Skip leading whitespace
    while (isspace(*line))
        line++;

    // If the line is empty after skipping whitespace
    if (*line == '\0')
    {
        token_start = NULL; // Reset token_start
        return;
    }

    size_t op_length = 0; // Length of the operator, if any
    TokenType op_type = operator_type(line, &op_length); // Determine the operator type and length

    if (op_type != TOKEN_WORD) // If the current character is an operator
    {
        token->type = op_type;
        token->full_text = strndup(line, op_length);
        token_start = line + op_length; // Move the start position for the next token

        token_add_segment(token, SEG_LITERAL, line, op_length, QUOTE_NONE);
        return;
    }

    if (*line == '~') // Check for tilde expansion (i.e. 1st character is a tilde)
        token->tilde_expand = true; // Mark the token for tilde expansion

    char *token_end = line; // Pointer to the end of the current token
    char *segment_start = line; // Pointer to the start of the current segment
    while (*token_end)
    {
        // Stop tokenizing at whitespace or operators if not in quotes or parentheses
        if (quote_state == QUOTE_NONE)
        {
            if (isspace(*token_end))
                break;

            if (is_operator_char(*token_end))
                break;
        }

        if (*token_end == '\'' && quote_state != QUOTE_DOUBLE)
        {
            flush_literal(token, segment_start, token_end, quote_state); // Flush the literal segment before the quote
            segment_start = token_end + 1; // Move the segment start to the character after
            quote_state = quote_state == QUOTE_NONE ? QUOTE_SINGLE : QUOTE_NONE; // Toggle single quote state
        }
        else if (*token_end == '\"' && quote_state != QUOTE_SINGLE)
        {
            flush_literal(token, segment_start, token_end, quote_state); // Flush the literal segment before the quote
            segment_start = token_end + 1; // Move the segment start to the character after
            quote_state = quote_state == QUOTE_NONE ? QUOTE_DOUBLE : QUOTE_NONE; // Toggle double quote state
        }
        else if (*token_end == '$' && *(token_end + 1) == '(' && *(token_end + 2) == '(')
        {
            flush_literal(token, segment_start, token_end, quote_state); // Flush the literal segment before the variable
            segment_start = token_end;
            token_end += 3; // Skip past the next three characters (the opening parentheses)

            int expr_length = expression(token_end); // Get the length of the expression inside $(())
            if (expr_length == -1) // Check for syntax error in expression
            {
                token_start = NULL; // Reset token_start
                return; // Return to indicate an error   
            }
            token_end += expr_length; // Skip past the end of the expression inside $(())

            if (*token_end != ')' || *(token_end + 1) != ')') // Ensure we have a closing parentheses
            {
                fprintf(stderr, "Error: Unmatched opening parenthesis in input");
                token_start = NULL; // Reset token_start
                return; // Return to indicate an error
            }

            token_end += 2; // Skip past the closing parentheses
            token_add_segment(token, SEG_ARITHMETIC, segment_start, token_end - segment_start, quote_state);
            segment_start = token_end; // Move the segment start to the character after
            continue;
        }
        else if (*token_end == '$' && *(token_end + 1) == '(')
        {
            flush_literal(token, segment_start, token_end, quote_state); // Flush the literal segment before the variable
            segment_start = token_end;
            token_end += 2; // Skip past the next two characters (the opening parenthesis)

            int cmd_length = command_substitution(token_end); // Get the length of the command substitution inside $()
            if (cmd_length == -1) // Check for syntax error in command substitution
            {
                token_start = NULL; // Reset token_start
                return; // Return to indicate an error   
            }
            token_end += cmd_length; // Skip past the end of the command substitution inside $()

            if (*token_end != ')') // Ensure we have a closing parenthesis
            {
                fprintf(stderr, "Error: Unmatched opening parenthesis in input");
                token_start = NULL; // Reset token_start
                return; // Return to indicate an error
            }

            token_end++; // Skip past the closing parenthesis
            token_add_segment(token, SEG_COMMAND, segment_start, token_end - segment_start, quote_state);
            segment_start = token_end; // Move the segment start to the character after
            continue;
        }
        else if (*token_end == '$' && (isalpha(*(token_end + 1)) || *(token_end + 1) == '_' || *(token_end + 1) == '$' || *(token_end + 1) == '?')) // Check for variable expansion
        {
            flush_literal(token, segment_start, token_end, quote_state); // Flush the literal segment before the variable
            segment_start = token_end;
            token_end++; // Move past the '$' character

            while (isalnum(*token_end) || *token_end == '_') // Move through the variable name
                token_end++;

            token_add_segment(token, SEG_VARIABLE, segment_start, token_end - segment_start, quote_state);
            segment_start = token_end; // Move the segment start to the character after
            continue;
        }
        else if (*token_end == '$' && *(token_end + 1) == '{') // Check for variable expansion with braces
        {
            flush_literal(token, segment_start, token_end, quote_state); // Flush the literal segment before the variable
            segment_start = token_end;
            token_end += 2; // Move past the '${' characters

            while (isalnum(*token_end) || *token_end == '_') // Move through the variable name
                token_end++;

            if (*token_end != '}') // Ensure we have a closing brace
            {
                fprintf(stderr, "Error: Unmatched opening brace in input");
                token_start = NULL; // Reset token_start
                return; // Return to indicate an error
            }

            token_end++; // Move past the closing brace
            token_add_segment(token, SEG_VARIABLE, segment_start, token_end - segment_start, quote_state);
            segment_start = token_end; // Move the segment start to the character after
            continue;
        }
        else if (*token_end == '\\')
        {
            if (*(token_end + 1) == '\0') // If the string ends with a backslash, it's an error
            {
                fprintf(stderr, "Error: Trailing backslash in input");
                token_start = NULL; // Reset token_start
                return; // Return to indicate an error
            }

            if (quote_state == QUOTE_SINGLE) // Backslashes are treated as literal characters inside single quotes
            {
                token_end++; // Move past the backslash
                continue;
            }
            
            // token->escaped = quote_state != QUOTE_DOUBLE && strchr("\\\"'$`", *(token_end + 1)) != NULL; // Check if the next character is escapable
            token_end++; // Skip the next character (escaped character will be treated as a literal)
        }
        else if (quote_state == QUOTE_NONE && *token_end == ')') // Stop tokenizing at closing parenthesis if not in quotes
        {
            fprintf(stderr, "syntax error near unexpected token `)'");
            token_start = NULL; // Reset token_start
            return; // Return to indicate an error
        }

        token_end++;
    }

    flush_literal(token, segment_start, token_end, quote_state); // Flush any remaining literal segment

    if (quote_state == QUOTE_DOUBLE)
    {
        fprintf(stderr, "Error: Unmatched double quotes in input");
        token_start = NULL; // Reset token_start
        return; // Return to indicate an error
    }

    if (quote_state == QUOTE_SINGLE)
    {
        fprintf(stderr, "Error: Unmatched single quotes in input");
        token_start = NULL; // Reset token_start
        return; // Return to indicate an error
    }

    // token_print(token); // Print the token for debugging purposes

    // If we reached the end of the line, return the last token
    if (*token_end == '\0')
    {
        token_start = token_end;
        return; // Return to indicate no more tokens
    }

    if (is_operator_char(*token_end)) // If we reached an operator, return the token and prepare for the next one
        token_start = token_end; // Set the start position for the next token
    else
        token_start = token_end + 1; // Update the start position for the next token
}

/**
 * Function: parse_line
 * ----------------------
 * Parses a line of input into an array of strings.
 *
 * This function takes a line of input and splits it into individual tokens based on whitespace.
 * It returns a dynamically allocated array of strings, where each string is a token from the input.
 *
 * Parameters:
 *   line - A pointer to the input string to be parsed.
 *
 * Returns:
 *   A pointer to a Command structure containing the parsed tokens and the number of tokens.
 * 
 * Note:
 *   The caller is responsible for freeing the memory allocated for the array of strings using free_command.
 * 
 */
Command *parse_line(char *line)
{
    size_t bufsize = INITIAL_TOKEN_CAPACITY;
    Command *command = malloc(sizeof(Command)); // Allocate memory for the Command structure
    if (!command)
    {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
    }

    command->argv = malloc(bufsize * sizeof(Token)); // Allocate memory for the array of token strings
    command->argc = 0; // Initialize the argument count to 0
    Token token;

    if (!command->argv)
    {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
    }

    tokenize_line(line, &token); // Get the first token from the input line
    while (token.segments != NULL)
    {
        command->argv[command->argc++] = token;

        // If we exceed the buffer size, reallocate more space
        if (command->argc >= bufsize)
        {
            bufsize += INITIAL_TOKEN_CAPACITY;
            command->argv = realloc(command->argv, bufsize * sizeof(Token));
            if (!command->argv)
            {
                fprintf(stderr, "Allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        tokenize_line(NULL, &token); // Continue tokenizing the input line
    }

    return command;
}

/*
 * Function: free_token
 * ----------------------
 * Frees the memory allocated for a Token structure and its segments.
 *
 * Parameters:
 *   token - A pointer to the Token structure to be freed.
 */
void free_token(Token *token)
{
    if (!token)
        return;

    for (size_t i = 0; i < token->segment_count; i++)
    {
        free(token->segments[i].text); // Free the text of each segment
        token->segments[i].text = NULL;
    }

    free(token->segments); // Free the segments array
    token->segments = NULL;

    free(token->full_text); // Free the full text of the token
    token->full_text = NULL;
}

/*
 * Function: free_tokens
 * ----------------------
 * Frees the memory allocated for an array of tokens and their segments.
 *
 * Parameters:
 *   tokens - A pointer to the array of tokens to be freed.
 *   count - The number of tokens in the array.
 */
static void free_tokens(Token *tokens, size_t count)
{
    for (size_t i = 0; i < count; i++)
        free_token(&tokens[i]); // Free each individual token
}

/**
 * Function: free_command
 * ----------------------
 * Frees the memory allocated for a Command structure and its tokens. This function takes 
 * a pointer to a Command structure and frees the memory allocated for each token, as well
 * as the memory allocated for the array itself and the Command structure.
 *
 * Parameters:
 *   cmd - A pointer to the Command structure containing the tokens to be freed.
 * 
 */
void free_command(Command *cmd)
{
    if (!cmd || !cmd->argv) // Check for NULL pointers
        return; // Nothing to free

    free_tokens(cmd->argv, cmd->argc); // Free the tokens and their associated memory
    free(cmd->argv); // Free the array of token pointers
    cmd->argv = NULL;
    free(cmd); // Free the Command structure itself
    cmd = NULL;
}
