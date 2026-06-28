/*
 *      shell.c
 *
 *      This file contains the implementation of a simple shell loop that reads user input and processes commands.
 *      It is responsible for printing the shell prompt, reading user input, and executing commands in a loop until the user exits.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "parser.h"
#include "shell.h"

/* 
 * Function: print_prompt
 * ----------------------
 * Prints the shell prompt to the standard output.
 *
 * This function displays the prompt to indicate that the shell is ready to accept user input.
 * It also flushes the output buffer to ensure that the prompt is displayed immediately.
 *
 */
static void print_prompt(void)
{
    printf("josh> ");
    fflush(stdout); // Ensure the prompt is displayed immediately
}

/*
 * Function: read_line
 * ---------------------
 * Reads a line of input from the standard input.
 *
 * This function reads a line of input from the standard input and returns it as a dynamically allocated string.
 * It is responsible for allocating memory for the input string and removing the trailing newline character.
 *
 * Returns:
 *   A pointer to the dynamically allocated string containing the user input, or NULL on error or EOF.
 */
static char *read_line(void)
{
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    read = getline(&line, &len, stdin);

    if (read == -1) // Check for EOF or error
    {
        free(line); // Free the allocated memory on error
        return NULL;
    }

    // Remove the trailing newline character if present
    if (line[read - 1] == '\n')
    {
        line[read - 1] = '\0';
    }

    return line;
}

/*
 * Function: shell_loop
 * --------------------
 * Implements the main loop of the shell.
 *
 * This function continuously prompts the user for input, reads a line of input, and processes it.
 * The loop continues until the user signals an exit (e.g., by pressing Ctrl+D).
 * The function also handles memory management for the input line and ensures that resources are freed appropriately.
 */
void shell_loop(void)
{
    while (1)
    {
        print_prompt();

        char *line = read_line();

        if (line == NULL) // Exit the loop on EOF or error
        {
            printf("\n");
            break;
        }

        char **tokens = parse_line(line); // Parse the input line into tokens

        free_tokens(tokens); // Free the memory allocated for tokens
        free(line); // Free the memory allocated by getline
    }
}
