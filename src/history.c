/*
 *      history.c
 *
 *      This file contains the implementation of command history functionality for the shell.
 *
 *      TODO:
 *       - Turn fixed-size array into a circular buffer to allow for an arbitrary number of commands in history.
 *       - Change data structure from a fixed-size array to a dynamic array using realloc to allow for an arbitrary number of commands in history.
 *       - Add history expansion functionality (e.g., !n, !!, !string) to allow users to recall and execute previous commands.
 *       - Implement persistent history storage by saving command history to a file and loading it on shell startup.
 *       - Implement arrow key navigation for command history to allow users to navigate through previous commands using the up and down arrow keys.
 *       - Add support for command timestamps to allow users to see when commands were executed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HISTORY_SIZE 100 // Maximum number of commands to store in history

static char *history[HISTORY_SIZE]; // Array to store command history
static int history_count = 0; // Current number of commands in history

/*
 * Function: history_init
 * ----------------------
 * Initializes the command history.
 *
 * This function initializes the command history by allocating memory for the history array and setting the initial size to zero.
 */
void history_init(void)
{
    history_count = 0;
}

/**
 * Function: history_add
 * ---------------------
 * Adds a command to the command history.
 *
 * Parameters:
 *   line - The command to add to the history
 * 
 * Note:
 *  If the history is full, the oldest command will be removed to make room for the new command.
 *  The command is duplicated using strdup to ensure that the history maintains its own copy of the
 *  command string since the input line might be deallocated by the caller. strdup allocates memory 
 *  for the new string, which should be freed when the history is destroyed.
 * 
 * TODO:
 *  - Implement a circular buffer or dynamic array to allow for an arbitrary number of commands in history.
 */
void history_add(const char *line)
{
    if (history_count >= HISTORY_SIZE) // if the history is full
    {
        // Remove the oldest command from history
        free(history[0]);
        for (int i = 0; i < HISTORY_SIZE - 1; i++) // Shift all commands to the left
        {
            history[i] = history[i + 1];
        }
        history_count--;
    }

    // Add the new command to the end of history
    history[history_count] = strdup(line);
    history_count++;
}

/*
 * Function: history_print
 * -----------------------
 * Prints the command history to the standard output.
 *
 * This function iterates through the command history and prints each command along with its index.
 * The commands are printed in the order they were added, with the oldest command first.
 */
void history_print(void)
{
    for (int i = 0; i < history_count; i++)
        printf("%d  %s\n", i + 1, history[i]); // Print the command index and command
}

/**
 * Function: history_destroy
 * -------------------------
 * Destroys the command history and frees all allocated memory.
 *
 * This function iterates through the command history and frees the memory allocated for each command string.
 * It then sets the history count to zero.
 */
void history_destroy(void)
{
    for (int i = 0; i < history_count; i++)
    {
        free(history[i]);
    }

    history_count = 0;
}