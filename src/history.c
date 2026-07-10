/*
 *      history.c
 *
 *      This file contains the implementation of command history functionality for the shell.
 *
 *      TODO:
 *       - Change data structure from a fixed-size array to a dynamic array using realloc to allow for an arbitrary number of commands in history.
 *       - Add history expansion functionality (e.g., !n, !!, !string) to allow users to recall and execute previous commands.
 *       - Add support for command timestamps to allow users to see when commands were executed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "history.h"

#define HISTORY_SIZE 100 // Maximum number of commands to store in history

static char *history[HISTORY_SIZE]; // Array to store command history
static int history_start = 0; // Index of the oldest command in history
static int history_count = 0; // Current number of commands in history
static int history_cursor = 0; // Cursor for navigating through history

/**
 * Function: get_history_file
 * --------------------------
 * Returns the path to the history file.
 */
static const char *get_history_file(void)
{
    const char *home = getenv("HOME");

    if (home == NULL)
    {
        fprintf(stderr, "Could not get HOME environment variable\n");
        return "/tmp/.josh_history"; // Fallback to a temporary file if HOME is not set
    }

    // Construct the path to the history file
    static char history_file[1024]; // static to ensure it persists after the function returns
    snprintf(history_file, sizeof(history_file), "%s/.josh_history", home);

    return history_file;
}

/*
 * Function: load_history
 * ----------------------
 * Loads command history from a file.
 */
static void load_history(void)
{
    const char *history_file = get_history_file();

    if (history_file == NULL)
    {
        fprintf(stderr, "Could not get history file path\n");
        return;
    }

    FILE *file = fopen(history_file, "r"); // Open the history file for reading
    if (file == NULL) // If the file does not exist, return without loading history
        return;

    char line[1024];
    while (fgets(line, sizeof(line), file)) // Read each line from the history file
    {
        // Remove the trailing newline character
        line[strcspn(line, "\n")] = '\0';
        history_add(line);
    }

    if (fclose(file) != 0) // Check for errors when closing the file
    {
        fprintf(stderr, "Error closing history file\n");
    }

}

/**
 * Function: save_history
 * ----------------------
 * Saves command history to a file.
 */
static void save_history(void)
{
    const char *history_file = get_history_file();

    if (history_file == NULL)
    {
        fprintf(stderr, "Could not get history file path\n");
        return;
    }

    FILE *file = fopen(history_file, "w"); // Open the history file for writing
    if (file == NULL)
    {
        fprintf(stderr, "Could not open history file for writing\n");
        return;
    }

    for (int i = 0; i < history_count; i++) // Iterate through the command history
    {
        int index = (history_start + i) % HISTORY_SIZE; // Calculate the index of the command in the circular buffer
        fprintf(file, "%s\n", history[index]); // Write each command to the history file
    }

    if (fclose(file) != 0) // Check for errors when closing the file
        fprintf(stderr, "Error closing history file\n");
}

/*
 * Function: history_init
 * ----------------------
 * Initializes the command history.
 *
 * This function initializes the command history by allocating memory for the history array and setting the initial size to zero.
 */
void history_init(void)
{
    history_start = 0;
    history_count = 0;
    history_cursor = 0;

    load_history();
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
 */
void history_add(const char *line)
{
    if (history_count >= HISTORY_SIZE) // if the history is full
    {
        // Remove the oldest command from history
        free(history[history_start]);
        history_start = (history_start + 1) % HISTORY_SIZE;
        history_count--;
    }

    // Add the new command to the history
    char *new_command = strdup(line); // Duplicate the command string

    if (new_command == NULL) // Check for memory allocation failure
    {
        fprintf(stderr, "Allocation error\n");
        // exit(EXIT_FAILURE);
        return; // Return without adding the command to history
    }
    history[(history_start + history_count) % HISTORY_SIZE] = new_command; // Store the duplicated command in history

    history_count++;
    history_cursor = 0; // Reset the history cursor when a new command is added
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
    {
        int index = (history_start + i) % HISTORY_SIZE; // Calculate the index of the command in the circular buffer
        printf("%d  %s\n", i + 1, history[index]); // Print the command index and command
    }
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
    save_history(); // Save the history to a file before destroying it

    for (int i = 0; i < history_count; i++)
    {
        int index = (history_start + i) % HISTORY_SIZE; // Calculate the index of the command in the circular buffer
        free(history[index]);
    }

    history_count = 0;
    history_cursor = 0;
}

const char *history_previous(void)
{
    if (history_count == 0) // If there is no history, return NULL
        return NULL;

    if (history_cursor < history_count) // If the cursor is not at the oldest command
        history_cursor++; // Move the cursor to the previous command

    int index = (history_start + history_count - history_cursor) % HISTORY_SIZE; // Calculate the index of the command in the circular buffer
    return history[index]; // Return the previous command
}

const char *history_next(void)
{
    if (history_count == 0) // If there is no history, return NULL
        return NULL;

    if (history_cursor > 0) // If the cursor is not at the newest command
        history_cursor--; // Move the cursor to the next command

    if (history_cursor == 0) // If the cursor is at the newest command, return an empty string
        return "";

    int index = (history_start + history_count - history_cursor) % HISTORY_SIZE; // Calculate the index of the command in the circular buffer
    return history[index]; // Return the next command
}
