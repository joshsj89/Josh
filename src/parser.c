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
    int paren_depth = 0;

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
    while (*token_end != '\0')
    {
        // Stop tokenizing at whitespace if not in quotes or parentheses
        if (!in_single_quotes && !in_double_quotes && paren_depth == 0 && isspace(*token_end))
            break; 

        if (*token_end == '\'' && !in_double_quotes && paren_depth == 0)
            in_single_quotes = !in_single_quotes; // Toggle single quote state
        else if (*token_end == '\"' && !in_single_quotes && paren_depth == 0)
            in_double_quotes = !in_double_quotes; // Toggle double quote state
        else if (*token_end == '(' && !in_single_quotes && !in_double_quotes)
            paren_depth++; // Increase parenthesis depth
        else if (*token_end == ')' && !in_single_quotes && !in_double_quotes)
            paren_depth--; // Decrease parenthesis depth
        else if (*token_end == '$' && *(token_end + 1) == '(' && *(token_end + 2) == '(')
        {
            paren_depth += 2; // Increase parenthesis depth for $((
            token_end += 2; // Skip the next two characters (the opening parentheses)
        }
        else if (*token_end == '$' && *(token_end + 1) == '(')
        {
            paren_depth++; // Increase parenthesis depth for $(
            token_end++; // Skip the next character (the opening parenthesis)
        }
        else if (*token_end == ')' && paren_depth > 0)
            paren_depth--; // Decrease parenthesis depth for )
        else if (*token_end == '\\')
            token_end++; // Skip the next character (escaped character will be treated as a literal)

        token_end++;
    }

    if (paren_depth != 0)
    {
        fprintf(stderr, "Error: Unmatched parentheses in input");
        token_start = NULL; // Reset token_start
        return NULL; // Return NULL to indicate an error
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