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
 * Function: find_pipe
 * --------------------
 * Finds the index of the pipe token in a command.
 *
 * Parameters:
 *   cmd - A pointer to the Command structure containing the command and its arguments
 *
 * Returns:
 *   The index of the pipe token if found, -1 otherwise
 */
static ssize_t find_pipe(Command *cmd)
{
    for (size_t i = 0; i < cmd->argc; i++)
        if (cmd->argv[i].type == TOKEN_PIPE)
            return i;

    return -1; // No pipe found
}

/*
 * Function: split_pipeline
 * -------------------------
 * Splits a command into two commands at the specified pipe index.
 *
 * Parameters:
 *   original - A pointer to the original Command structure containing the command and its arguments
 *   left - A pointer to the Command structure that will hold the left side of the pipe
 *   right - A pointer to the Command structure that will hold the right side of the pipe
 *   pipe_index - The index of the pipe token in the original command
 *
 * Returns:
 *   0 if successful, -1 if an error occurs (e.g., invalid pipe index)
 */
static int split_pipeline(Command *original, Command *left, Command *right, ssize_t pipe_index)
{
    if (pipe_index < 0 || pipe_index >= (ssize_t)original->argc) // Invalid pipe index
        return -1;

    left->argv = &original->argv[0];
    left->argc = pipe_index;

    right->argv = &original->argv[pipe_index + 1];
    right->argc = original->argc - pipe_index - 1;

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
 * Function: execute_pipeline
 * --------------------------
 * Executes a pipeline of commands with the given pipe index.
 *
 * Parameters:
 *   cmd - A pointer to the Command structure containing the pipeline
 *   pipe_index - The index of the pipe in the command array
 */
static void execute_pipeline(Command *cmd, ssize_t pipe_index)
{
    Command left, right;
    if (split_pipeline(cmd, &left, &right, pipe_index) == -1)
    {
        fprintf(stderr, "Invalid pipe index\n");
        return; // Return if splitting the pipeline fails
    }

    int pipefd[2]; // index 0 for reading, index 1 for writing
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        return;
    }

    pid_t left_pid = fork(); // Fork the left command
    if (left_pid < 0) // if fork fails
    {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);

        return;
    }
    else if (left_pid == 0) // Child process for the left command
    {
        close(pipefd[0]); // Close the read end of the pipe in the left child
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to the write end of the pipe
        close(pipefd[1]); // Close the write end after duplicating

        execute_child(&left); // Execute the left command

        exit(EXIT_FAILURE); // Will only reach here if execvp fails
    }

    pid_t right_pid = fork(); // Fork the right command
    if (right_pid < 0) // if fork fails
    {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);

        waitpid(left_pid, NULL, 0); // Wait for the left child to finish if it was created
        return;
    }
    else if (right_pid == 0) // Child process for the right command
    {
        close(pipefd[1]); // Close the write end of the pipe in the right child
        dup2(pipefd[0], STDIN_FILENO); // Redirect stdin to the read end of the pipe
        close(pipefd[0]); // Close the read end after duplicating

        execute_child(&right); // Execute the right command

        exit(EXIT_FAILURE); // Will only reach here if execvp fails
    }

    // Close both ends of the pipe in the parent process
    close(pipefd[0]);
    close(pipefd[1]);

    int status;
    waitpid(left_pid, &status, 0); // Wait for the left child to finish
    waitpid(right_pid, &status, 0); // Wait for the right child to finish
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
        return; // No command entered, return without doing anything

    ssize_t pipe_index = find_pipe(cmd); // Check if there is a pipe in the command
    if (pipe_index != -1)
    {
        execute_pipeline(cmd, pipe_index); // Execute the command as a pipeline if a pipe is found
        return;
    }

    if (execute_builtin(cmd)) // Check if the command is a built-in command
        return; // Built-in command executed, return without creating a child process

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
}
