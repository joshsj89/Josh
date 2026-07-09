/*
    * File: execute.c
    * ----------------
    * This file contains the implementation of the execute_command function,
    * which is responsible for executing commands with their respective arguments.
    * 
    * TODO:
    * - Implement support for input/output redirection, piping, and background jobs (&).
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> // for pid_t
#include <sys/wait.h> // for waitpid
#include <unistd.h> // for fork, execvp
#include "builtins.h"
#include "execute.h"
#include "tokens.h"

/**
 * Function: command_to_argv
 * --------------------------
 * Converts a Command structure to an array of strings suitable for execvp.
 *
 * Parameters:
 *   cmd - A pointer to the Command structure containing the command and its arguments
 *
 * Returns:
 *   A pointer to an array of strings representing the command and its arguments,
 *   or NULL if an error occurs.
 * 
 * Note:
 *   The caller is responsible for freeing the memory allocated for the array of strings.
 */
char **command_to_argv(const Command *cmd)
{
    if (cmd == NULL || cmd->argv == NULL || cmd->argc == 0)
        return NULL; // No command to convert

    char **argv = malloc((cmd->argc + 1) * sizeof(char *)); // +1 for NULL termination
    if (argv == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL; // Return NULL to indicate an error
    }

    for (size_t i = 0; i < cmd->argc; i++)
        if (cmd->argv[i].type == TOKEN_WORD)
            argv[i] = cmd->argv[i].full_text; // Assign the text of each token to argv

    argv[cmd->argc] = NULL; // Null-terminate the array

    return argv;
}

/*
 * Function: execute_command
 * -------------------------
 * Executes a command with the given arguments. This function is responsible for
 * making sure there is a command to execute, creating a child process to run the
 * command, replacing the child process with the command using execvp, and waiting
 * for the child process to finish execution. It also handles errors that may occur during the execution process.
 *
 * Parameters:
 *   cmd - A pointer to the Command structure containing the command and its arguments
 * 
 * Note:
 *  The caller is responsible for ensuring that the args array is properly allocated and terminated with a NULL pointer
 *  as the parsing should have already handled this.
 */
void execute_command(Command *cmd)
{
    // Check if there is a command to execute
    if (cmd == NULL || cmd->argv == NULL || cmd->argc == 0 || cmd->argv[0].full_text == NULL)
    {
        return; // No command entered, return without doing anything
    } // No command entered, return without doing anything

    if (execute_builtin(cmd)) // Check if the command is a built-in command
    {
        return; // Built-in command executed, return without creating a child process
    }

    // Create a child process to execute the command
    pid_t pid = fork();
    if (pid < 0) // error
    {
        perror("fork"); // Handle fork error
        exit(EXIT_FAILURE);
    }
    else if (pid == 0) // child process
    {
        // In the child process, replace it with the command using execvp
        char **argv = command_to_argv(cmd); // Convert Command to argv array
        if (execvp(cmd->argv[0].full_text, argv) < 0) // execvp returns only on error
        {
            if (errno == ENOENT && strchr(cmd->argv[0].full_text, '/') == NULL) // Command not found and no '/' in command
                fprintf(stderr, "%s: command not found\n", cmd->argv[0].full_text); // Handle command not found error
            else
                perror(cmd->argv[0].full_text); // Handle execvp error

            free(argv); // Free the allocated argv array before exiting
            exit(EXIT_FAILURE);
        }
    }
    else // parent process
    {
        // In the parent process, wait for the child process to finish execution
        int status;
        waitpid(pid, &status, 0);
    }
}
