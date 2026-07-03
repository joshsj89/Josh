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
 *   cmd - A pointer to the Command structure containing the command and its arguments
 *
 * Returns:
 *   1 if the command was executed successfully
 * 
 * TODO:
 *  - Implement handling for "cd -" and "cd ~" to change to the previous directory and home directory, respectively.
 */
int shell_cd(Command *cmd)
{
    if (cmd->argv[1].text == NULL) // no argument provided
    {
        if (chdir(getenv("HOME")) != 0) // change to home directory
        {
            fprintf(stderr, "cd: %s\n", strerror(errno));
        }
    }
    else
    {
        if (chdir(cmd->argv[1].text) != 0)
            fprintf(stderr, "cd: %s: %s\n", cmd->argv[1].text, strerror(errno));
    }

    return 1; // Return 1 to indicate that the shell should continue running
}

/*
 * shell_exit - Exit the shell
 *
 * Parameters:
 *   cmd - A pointer to the Command structure containing the command and its arguments
 *
 * Returns:
 *   This function does not return as it exits the shell.
 */
int shell_exit(Command *cmd)
{
    (void)cmd; // Unused parameter
    history_destroy(); // Clean up history before exiting
    exit(EXIT_SUCCESS); // Exit the shell with status code 0
}

/*
 * execute_builtin - Execute a built-in command
 * 
 * Parameters:
 *   cmd - A pointer to the Command structure containing the command and its arguments
 *
 * Returns:
 *   1 if the command was executed successfully
 *   0 if the command is not a built-in
 * 
 * TODO:
 * - Add more built-in commands as needed (e.g., "help", "jobs", "fg", "bg", "alias", "unalias", etc.)
 */
int execute_builtin(Command *cmd)
{
    if (cmd == NULL || cmd->argv == NULL || cmd->argc == 0 || cmd->argv[0].text == NULL)
    {
        return 0; // No command entered
    }

    if (strcmp(cmd->argv[0].text, "cd") == 0)
    {
        return shell_cd(cmd);
    }
    else if (strcmp(cmd->argv[0].text, "exit") == 0)
    {
        return shell_exit(cmd);
    }
    else if (strcmp(cmd->argv[0].text, "history") == 0)
    {
        history_print();
        return 1; // Indicate that the command was executed successfully
    }

    return 0; // Command is not a built-in
}
