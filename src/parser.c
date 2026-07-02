/*
 *      parser.c
 *
 *      This file contains the implementation of the command parser for the shell.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

#define INITIAL_TOKEN_CAPACITY 64 // Initial capacity for the token array

/**
 * Parses a Bash expression, tracks parenthesis depth
 * and verifies syntax correctness.
 *
 * @param input The input Bash expression string.
 * @return The length of the expression if parentheses are balanced, -1 otherwise.
 * @note This function assumes the input string begins at the first character of the expression and does not skip leading whitespace.
 * The length returned is the number of characters processed in the expression. 
 * input + length will point to the character after the expression, which is useful for further processing of the input string.
 */
static int expression(const char *input)
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

/**
 * Parses a simple command, handling quotes and escape characters.
 *
 * @param input The input string for the simple command.
 * @return The length of the simple command if parsed successfully, -1 otherwise.
 */
int simple_command(const char *input)
{
    int length = 0; // Track the length of the simple command
    int in_single = 0; // Track if we are inside single quotes
    int in_double = 0; // Track if we are inside double quotes

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

                continue;
            }
            
            if (*input == '$' && *(input + 1) == '(')
            {
                input += 2; // Move past the '$' and the '(' characters
                length += 2; // Count the '$' and the '(' characters
                
                int cmd_length = simple_command(input); // Get the length of the simple command inside $()
                if (cmd_length == -1) // Check for syntax error in simple command
                    return -1; // Return -1 to indicate an error

                input += cmd_length; // Skip to the end of the simple command inside $()
                length += cmd_length; // Count the characters in the simple command

                continue;
            }
            
            if (*input == ')')
                break;
        }

        length++; // Increment the length of the simple command
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

    return length; // Return the length of the simple command
}

/*
 * Function: tokenize_line
 * ------------------------
 * Tokenizes a line of input into individual tokens.
 *
 * This function takes a line of input and splits it into individual tokens.
 * It returns a dynamically allocated string containing the next token from the input.
 *
 * Parameters:
 *   line - A pointer to the input string to be tokenized.
 *
 * Returns:
 *   A pointer to a dynamically allocated string containing the next token, or NULL if no more tokens are available.
 */
static char *tokenize_line(char *line)
{
    int in_single_quotes = 0;
    int in_double_quotes = 0;

    static char *token_start = NULL; // Pointer to the start of the current token

    // Continue from the last position if line is NULL
    if (line == NULL)
        line = token_start;

     // If the line is empty, reset token_start and return NULL
    if (*line == '\0')
    {
        token_start = NULL;
        return NULL;
    }

    // Skip leading whitespace
    while (isspace(*line))
        line++;

    // If the line is empty after skipping whitespace
    if (*line == '\0') 
    {
        token_start = NULL; // Reset token_start
        return NULL;
    }

    char *token_end = line; // Pointer to the end of the current token
    while (*token_end)
    {
        // Stop tokenizing at whitespace if not in quotes or parentheses
        if (!in_single_quotes && !in_double_quotes && isspace(*token_end))
            break; 

        if (*token_end == '\'' && !in_double_quotes)
            in_single_quotes = !in_single_quotes; // Toggle single quote state
        else if (*token_end == '\"' && !in_single_quotes)
            in_double_quotes = !in_double_quotes; // Toggle double quote state
        else if (*token_end == '$' && *(token_end + 1) == '(' && *(token_end + 2) == '(')
        {
            token_end += 3; // Skip past the next three characters (the opening parentheses)

            int expr_length = expression(token_end); // Get the length of the expression inside $(())
            if (expr_length == -1) // Check for syntax error in expression
            {
                token_start = NULL; // Reset token_start
                return NULL; // Return NULL to indicate an error   
            }
            token_end += expr_length; // Skip past the end of the expression inside $(())

            if (*token_end != ')' || *(token_end + 1) != ')') // Ensure we have a closing parentheses
            {
                fprintf(stderr, "Error: Unmatched opening parenthesis in input");
                token_start = NULL; // Reset token_start
                return NULL; // Return NULL to indicate an error
            }

            token_end += 2; // Skip past the closing parentheses
            continue;
        }
        else if (*token_end == '$' && *(token_end + 1) == '(')
        {
            token_end += 2; // Skip past the next two characters (the opening parenthesis)

            int cmd_length = simple_command(token_end); // Get the length of the simple command inside $()
            if (cmd_length == -1) // Check for syntax error in simple command
            {
                token_start = NULL; // Reset token_start
                return NULL; // Return NULL to indicate an error   
            }
            token_end += cmd_length; // Skip past the end of the simple command inside $()

            if (*token_end != ')') // Ensure we have a closing parenthesis
            {
                fprintf(stderr, "Error: Unmatched opening parenthesis in input");
                token_start = NULL; // Reset token_start
                return NULL; // Return NULL to indicate an error
            }

            token_end++; // Skip past the closing parenthesis
            continue;
        }
        else if (*token_end == '\\')
            token_end++; // Skip the next character (escaped character will be treated as a literal)
        else if (!in_single_quotes && !in_double_quotes && *token_end == ')') // Stop tokenizing at closing parenthesis if not in quotes
        {
            fprintf(stderr, "syntax error near unexpected token `)'");
            token_start = NULL; // Reset token_start
            return NULL; // Return NULL to indicate an error
        }

        token_end++;
    }

    if (in_double_quotes)
    {
        fprintf(stderr, "Error: Unmatched double quotes in input");
        token_start = NULL; // Reset token_start
        return NULL; // Return NULL to indicate an error
    }

    if (in_single_quotes)
    {
        fprintf(stderr, "Error: Unmatched single quotes in input");
        token_start = NULL; // Reset token_start
        return NULL; // Return NULL to indicate an error
    }

    // If we reached the end of the line, return the last token
    if (*token_end == '\0')
    {
        token_start = token_end;
        return line;
    }

    *token_end = '\0'; // Terminate the token
    token_start = token_end + 1; // Update the start position for the next token
    return line;
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
 *   A pointer to an array of strings containing the parsed tokens, or NULL on error.
 * 
 * Note:
 *   The caller is responsible for freeing the memory allocated for the array of strings using free_tokens.
 *   strtok() does not allocate new memory for tokens; it modifies the input string in place. Therefore,
 *   the caller should only free the array of strings, not the individual tokens.
 * 
 * TODO:
 *  - Handle tokens that are quoted or escaped.
 *      - This might involve using something other than strtok()
 */
char **parse_line(char *line)
{
    int bufsize = INITIAL_TOKEN_CAPACITY;
    int position = 0; // Current position in the token array
    char **tokens = malloc(bufsize * sizeof(char *)); // Allocate memory for the array of token strings
    char *token;

    if (!tokens)
    {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = tokenize_line(line); // Get the first token from the input line
    while (token != NULL)
    {
        tokens[position] = token;
        position++;

        // If we exceed the buffer size, reallocate more space
        if (position >= bufsize)
        {
            bufsize += INITIAL_TOKEN_CAPACITY;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens)
            {
                fprintf(stderr, "Allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = tokenize_line(NULL); // Continue tokenizing the input line
    }
    tokens[position] = NULL; // Null-terminate the array of tokens

    // Print the tokens for debugging purposes
    // for (int i = 0; i < position; i++)
    // {
    //     printf("Token %d: %s\n", i, tokens[i]);
    // }

    return tokens;
}

/**
 * Function: free_tokens
 * ----------------------
 * Frees the memory allocated for an array of strings.
 *
 * This function takes a pointer to an array of strings and frees the memory allocated for each string,
 * as well as the memory allocated for the array itself.
 *
 * Parameters:
 *   tokens - A pointer to the array of strings to be freed.
 * 
 * TODO:
 *  - Once strtok() is no longer used in parse_line(), update this function to free each individual token string as well.
 */
void free_tokens(char **tokens)
{
    if (tokens == NULL)
        return; // Nothing to free

    // Free the array of token pointers
    free(tokens);
}