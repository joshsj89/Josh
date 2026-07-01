/*
 * builtins.c
 *
 * This file contains the implementation of shell built-in commands.
 */

#include <errno.h> // for errno
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for chdir
#include "builtins.h"
#include "history.h"

/*
 * shell_cd - Change the current directory
 *
 * Parameters: 
 *   args - Array of strings representing the command and its arguments
 *
 * Returns:
 *   1 if the command was executed successfully
 * 
 * TODO:
 *  - Implement handling for "cd -" and "cd ~" to change to the previous directory and home directory, respectively.
 */
int shell_cd(char **args)
{
    if (args[1] == NULL) // no argument provided
    {
        if (chdir(getenv("HOME")) != 0) // change to home directory
        {
            fprintf(stderr, "cd: %s\n", strerror(errno));
        }
    }
    else
    {
        if (chdir(args[1]) != 0)
            fprintf(stderr, "cd: %s: %s\n", args[1], strerror(errno));
    }

    return 1; // Return 1 to indicate that the shell should continue running
}

/*
 * shell_exit - Exit the shell
 *
 * Parameters:
 *   args - Array of strings representing the command and its arguments
 *
 * Returns:
 *   This function does not return as it exits the shell.
 */
int shell_exit(char **args)
{
    (void)args; // Unused parameter
    history_destroy(); // Clean up history before exiting
    exit(EXIT_SUCCESS); // Exit the shell with status code 0
}

/*
 * execute_builtin - Execute a built-in command
 * 
 * Parameters:
 *   args - Array of strings representing the command and its arguments
 *
 * Returns:
 *   1 if the command was executed successfully
 *   0 if the command is not a built-in
 * 
 * TODO:
 * - Add more built-in commands as needed (e.g., "help", "jobs", "fg", "bg", "alias", "unalias", etc.)
 */
int execute_builtin(char **args)
{
    if (args == NULL || args[0] == NULL)
    {
        return 0; // No command entered
    }

    if (strcmp(args[0], "cd") == 0)
    {
        return shell_cd(args);
    }
    else if (strcmp(args[0], "exit") == 0)
    {
        return shell_exit(args);
    }
    else if (strcmp(args[0], "history") == 0)
    {
        history_print();
        return 1; // Indicate that the command was executed successfully
    }

    return 0; // Command is not a built-in
}
