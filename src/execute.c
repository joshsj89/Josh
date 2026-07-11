/*
 * File: execute.c
 * ----------------
 * This file contains the implementation of the execute_command function,
 * which is responsible for executing commands with their respective arguments.
 * 
 * TODO:
 * - Implement support for chaining piping and background jobs (&).
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> // for pid_t
#include <sys/wait.h> // for waitpid
#include <unistd.h> // for fork, execvp
#include "builtins.h"
#include "execute.h"
#include "tokens.h"

#define MAX_COMMANDS 64 // Maximum number of commands in a pipeline

/*
 * Function: is_redirection
 * -------------------------
 * Checks if a token represents a redirection operator.
 *
 * Parameters:
 *   type - The type of the token to check
 *
 * Returns:
 *   true if the token is a redirection operator, false otherwise
 */
static bool is_redirection(TokenType type)
{
    return type == TOKEN_REDIRECT_IN || 
            type == TOKEN_REDIRECT_OUT || 
            type == TOKEN_APPEND;
}

/*
 * Function: apply_redirections
 * -----------------------------
 * Applies input/output redirections to the command.
 *
 * Parameters:
 *   cmd - A pointer to the Command structure containing the command and its arguments
 *
 * Returns:
 *   0 if successful, -1 if an error occurs
 */
static int apply_redirections(Command *cmd)
{
    for (size_t i = 0; i < cmd->argc; i++)
    {
        Token *token = &cmd->argv[i];

        if (is_redirection(token->type))
        {
            if (i + 1 >= cmd->argc || cmd->argv[i + 1].type != TOKEN_WORD)
            {
                fprintf(stderr, "syntax error: expected filename after '%c'\n", token->type);
                return -1;
            }

            const char *filename = cmd->argv[i + 1].full_text;
            int fd;

            if (token->type == TOKEN_REDIRECT_IN)
                fd = open(filename, O_RDONLY);
            else if (token->type == TOKEN_REDIRECT_OUT)
                fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            else if (token->type == TOKEN_APPEND)
                fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644); 
            else
                continue;

            if (fd < 0)
            {
                perror("open");
                return -1;
            }

            if (token->type == TOKEN_REDIRECT_IN)
            {
                if (dup2(fd, STDIN_FILENO) < 0)
                {
                    perror("dup2");
                    close(fd);
                    return -1;
                }
            }
            else // TOKEN_REDIRECT_OUT or TOKEN_APPEND
            {
                if (dup2(fd, STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    close(fd);
                    return -1;
                }
            }

            close(fd); // Close the file descriptor after duplicating
            i++; // Skip the next token since it's the filename
        }
    }

    return 0;
}

/*
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
static char **command_to_argv(const Command *cmd)
{
    if (cmd == NULL || cmd->argv == NULL || cmd->argc == 0)
        return NULL; // No command to convert

    char **argv = malloc((cmd->argc + 1) * sizeof(char *)); // +1 for NULL termination
    if (argv == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL; // Return NULL to indicate an error
    }

    size_t j = 0; // used to index into argv, skipping non-word tokens
    for (size_t i = 0; i < cmd->argc; i++)
    {
        if (is_redirection(cmd->argv[i].type)) 
        {
            i++; // Skip the next token (filename for redirection)
            continue;
        }

        argv[j++] = cmd->argv[i].full_text; // Assign the text of each token to argv
    }

    argv[j] = NULL; // Null-terminate the array

    return argv;
}

/*
 * Function: execute_child
 * ------------------------
 * Executes a command in a child process.
 *
 * Parameters:
 *   cmd - A pointer to the Command structure containing the command and its arguments
 */
static void execute_child(Command *cmd)
{
    if (apply_redirections(cmd) == -1) // Apply any input/output redirections
            exit(EXIT_FAILURE); // Exit if redirection fails
        
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

/*
 * Function: split_commands
 * ------------------------
 * Splits a command into multiple commands based on pipes.
 *
 * Parameters:
 *   input - A pointer to the input Command structure
 *   commands - A pointer to an array of Command structures
 *
 * Returns:
 *   The number of commands split.
 */
size_t split_commands(Command *input, Command *commands)
{
    size_t start = 0;
    size_t count = 0;

    for (size_t i = 0; i < input->argc; i++)
    {
        Token *token = &input->argv[i];

        if (token->type == TOKEN_PIPE)
        {
            commands[count].argv = &input->argv[start];
            commands[count].argc = i - start;
            count++;
            start = i + 1; // Move start to the next token after the pipe
        }
    }

    // Handle the last command after the last pipe (or if there are no pipes)
    if (start < input->argc)
    {
        commands[count].argv = &input->argv[start];
        commands[count].argc = input->argc - start;
        count++;
    }

    return count;
}

/*
 * Function: execute_pipeline
 * --------------------------
 * Executes a series of commands connected by pipes.
 * 
 * Parameters:
 *   cmds - A pointer to an array of Command structures representing the commands in the pipeline
 *   num_cmds - The number of commands in the pipeline
 * 
 * Note:
 *   This function creates a new process for each command in the pipeline, sets up the necessary
 *   pipes for inter-process communication, and waits for all child processes to finish.
 * 
 */
static void execute_pipeline(Command *cmds, size_t num_cmds)
{
    int in_fd = 0; // Start with standard input
    int fd[2];

    for (size_t i = 0; i < num_cmds; i++)
    {
        if (i < num_cmds - 1) // If not the last command, create a pipe
        {
            if (pipe(fd) == -1)
            {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) // Child process
        {
            if (in_fd != 0) // If not the first command, redirect input
            {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            if (i < num_cmds - 1) // If not the last command, redirect output to the pipe
            {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }

            execute_child(&cmds[i]); // Execute the command in the child process
        }
        else // Parent process
        {
            if (in_fd != 0)
                close(in_fd); // Close the previous read end

            if (i < num_cmds - 1)
                close(fd[1]); // Close the write end of the pipe in the parent

            in_fd = fd[0]; // Save the read end for the next command
        }
    }

    // Wait for all child processes to finish
    for (size_t i = 0; i < num_cmds; i++)
        wait(NULL);
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
 * Returns:
 *  0 if the command was executed successfully, -1 if the shell should exit.
 * 
 * Note:
 *  The caller is responsible for ensuring that the args array is properly allocated and terminated with a NULL pointer
 *  as the parsing should have already handled this.
 */
int execute_command(Command *cmd)
{
    // Check if there is a command to execute
    if (cmd == NULL || cmd->argv == NULL || cmd->argc == 0 || cmd->argv[0].full_text == NULL)
        return 0; // No command entered, return without doing anything

    Command commands[MAX_COMMANDS]; // Array to hold split commands for pipelines
    size_t num_commands = split_commands(cmd, commands); // Count the number of commands in the pipeline
    if (num_commands > 1) // If there are multiple commands, execute as a pipeline
    {
        execute_pipeline(commands, num_commands);
        return 0;
    }

    int builtin_status = execute_builtin(cmd); // Check if the command is a built-in command
    if (builtin_status == -1)
        return -1; // exit the shell
    else if (builtin_status == 1)
        return 0;

    // Create a child process to execute the command
    pid_t pid = fork();
    if (pid < 0) // error
    {
        perror("fork"); // Handle fork error
        exit(EXIT_FAILURE);
    }
    else if (pid == 0) // child process
    {
        execute_child(cmd); // Execute the command in the child process
    }
    else // parent process
    {
        // In the parent process, wait for the child process to finish execution
        int status;
        waitpid(pid, &status, 0);
    }

    return 0;
}
