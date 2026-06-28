/*
 *      parser.c
 *
 *      This file contains the implementation of the command parser for the shell.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strtok
#include "parser.h"

#define TOKEN_DELIMITERS " \t\r\n\a" // Delimiters used to split the input line into tokens
#define INITIAL_TOKEN_CAPACITY 64 // Initial capacity for the token array

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

    token = strtok(line, TOKEN_DELIMITERS); // Get the first token from the input line
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

        token = strtok(NULL, TOKEN_DELIMITERS); // Continue tokenizing the input line
    }
    tokens[position] = NULL; // Null-terminate the array of tokens

    // Print the tokens for debugging purposes
    for (int i = 0; i < position; i++)
    {
        printf("Token %d: %s\n", i, tokens[i]);
    }

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