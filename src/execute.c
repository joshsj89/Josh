/*
    * File: execute.c
    * ----------------
    * This file contains the implementation of the execute_command function,
    * which is responsible for executing commands with their respective arguments.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> // for pid_t
#include <sys/wait.h> // for waitpid
#include <unistd.h> // for fork, execvp

/*
 * Function: execute_command
 * -------------------------
 * Executes a command with the given arguments. This function is responsible for
 * making sure there is a command to execute, creating a child process to run the
 * command, replacing the child process with the command using execvp, and waiting
 * for the child process to finish execution. It also handles errors that may occur during the execution process.
 *
 * Parameters:
 *   args - An array of strings representing the command and its arguments.
 * 
 * Note:
 *  The caller is responsible for ensuring that the args array is properly allocated and terminated with a NULL pointer
 *  as the parsing should have already handled this.
 */
void execute_command(char **args)
{
    // Check if there is a command to execute
    if (args == NULL || args[0] == NULL)
    {
        return; // No command entered, return without doing anything
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
        if (execvp(args[0], args) < 0) // execvp returns only on error
        {
            perror(args[0]); // Handle execvp error
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
