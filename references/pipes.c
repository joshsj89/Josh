#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void execute_pipeline(char ***commands, int num_commands)
{
    int in_fd = 0; // Starts at 0 (stdin)
    int fd[2];

    for (int i = 0; i < num_commands; i++)
    {
        pipe(fd); // Create a new pipe for the next command

        if (fork() == 0)
        {
            // --- Child Process ---
            // 1. Redirect STDIN to the previous pipe's read-end
            if (in_fd != 0)
            {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            // 2. Redirect STDOUT to the new pipe's write-end (if not the last command)
            if (i < num_commands - 1)
                dup2(fd[1], STDOUT_FILENO);

            // Clean up inherited or unused fds before execvp
            close(fd[0]);
            close(fd[1]);

            execvp(commands[i][0], commands[i]); // Execute the command
            perror("execvp failed");             // If execvp returns, it failed
            exit(EXIT_FAILURE);                  // Exit child process on failure
        }
        else
        {
            // --- Parent Process ---
            close(fd[1]); // Parent doesn't write to this pipe, close it

            // If not the first command, close previous input
            if (in_fd != 0)
                close(in_fd);

            // Save read-end for the next iteration (acts as the next `in_fd`)
            in_fd = fd[0];
        }
    }
}

int main(int argc, char *argv[])
{
    // Example usage of the pipeline execution logic
    // Assume args is an array of command arguments for each command in the pipeline
    char *args1[] = {"ls", "-l", NULL};
    char *args2[] = {"grep", "txt", NULL};
    char *args3[] = {"wc", "-l", NULL};

    char **commands[] = {args1, args2, args3};
    int num_commands = sizeof(commands) / sizeof(commands[0]);

    execute_pipeline(commands, num_commands);

    return 0;
}
