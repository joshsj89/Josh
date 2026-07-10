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
#include "execute.h"
#include "expand.h"
#include "history.h"
#include "line_editor.h"
#include "parser.h"
#include "prompt.h"
#include "shell.h"
                                                                          
const char *banner = "\n"
    "\x1b[34m       █████                  █████     \x1b[0m\n"
    "\x1b[34m      ▒▒███                  ▒▒███      \x1b[0m\n"
    "\x1b[34m       ▒███   ██████   █████  ▒███████  \x1b[0m\n"
    "\x1b[34m       ▒███  ███▒▒███ ███▒▒   ▒███▒▒███ \x1b[0m\n"
    "\x1b[34m       ▒███ ▒███ ▒███▒▒█████  ▒███ ▒███ \x1b[0m\n"
    "\x1b[34m ███   ▒███ ▒███ ▒███ ▒▒▒▒███ ▒███ ▒███ \x1b[0m\n"
    "\x1b[34m▒▒████████  ▒▒██████  ██████  ████ █████\x1b[0m\n"
    " \x1b[34m ▒▒▒▒▒▒▒▒    ▒▒▒▒▒▒  ▒▒▒▒▒▒  ▒▒▒▒ ▒▒▒▒▒ \x1b[0m\n"
    "                                        ";

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
    fputs(get_prompt(), stdout);
    
    fflush(stdout); // Ensure the prompt is displayed immediately
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
    history_init(); // Initialize the command history

    // Banner message for the shell
    printf("Welcome to Josh!\n");
    printf("%s\n", banner);
    printf("A simple shell, built from scratch in C.\n");
    printf("------------------------------------------------\n");
    printf("🐚 \x1b[36mShell:\x1b[0m Josh v0.2.2\n");
    printf("Type 'exit' or press Ctrl+D to exit the shell.\n");
    printf("------------------------------------------------\n");

    while (1)
    {
        print_prompt();

        char *line = line_editor_read();

        if (line == NULL) // Exit the loop on EOF or error
        {
            printf("\n");
            break;
        }

        history_add(line); // Add the line to command history

        Command *cmd = parse_line(line); // Parse the input line into a command

        expand_variables(cmd); // Expand any variables in the command

        execute_command(cmd); // Execute the parsed command

        free_command(cmd); // Free the memory allocated for the command
        free(line); // Free the memory allocated by line_editor_read
    }

    history_destroy(); // Clean up the command history before exiting
}
