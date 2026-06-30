/*
 *      File: completion.c
 *      -------------------
 *      This file is intended to handle tab completion functionality for the shell.
 *
 */

#include <dirent.h> // For directory operations
#include <linux/limits.h> // For PATH_MAX
#include <stdio.h> // For snprintf
#include <stdlib.h> // For free and getenv
#include <string.h>
#include <sys/ioctl.h> // For terminal size
#include <unistd.h> // For access()
#include "completion.h"

#define MAX_MATCHES 100 // Maximum number of matches to store

static int previous_was_tab = 0; // Flag to track if the previous key pressed was a tab
static char last_prefix[256] = ""; // Buffer to store the last prefix used for completion

struct winsize ws; // Structure to hold terminal window size

/*
 * Function: compare_strings
 * ---------------------------
 * Compares two strings for sorting purposes.
 *
 * Parameters:
 *   a - Pointer to the first string.
 *   b - Pointer to the second string.
 *
 * Returns:
 *   An integer indicating the result of the comparison.
 */
static int compare_strings(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

/*
 * Function: is_command_position
 * -------------------------------
 * Determines if the cursor is at the position of a command.
 *
 * Parameters:
 *   buffer - The input buffer containing the line of text.
 *   cursor_position - The position of the cursor in the buffer.
 *
 * Returns:
 *   1 if the cursor is at the command position, 0 otherwise.
 */
static int is_command_position(const char *buffer, size_t cursor_position)
{
    for (size_t i = 0; i < cursor_position; i++)
    {
        if (buffer[i] == ' ')
            return 0; // Not at the command position
    }

    return 1; // At the command position
}

/*
 * Function: match_exists
 * ------------------------
 * Checks if a match already exists in the list of matches.
 *
 * Parameters:
 *   matches - The buffer containing the list of matches.
 *   match_count - The number of matches currently in the buffer.
 *   new_match - The match to check for existence.
 *
 * Returns:
 *   1 if the match exists, 0 otherwise.
 */
static int match_exists(char matches[][256], int match_count, const char *new_match)
{
    for (int i = 0; i < match_count; i++)
    {
        if (strcmp(matches[i], new_match) == 0)
            return 1; // Match already exists
    }

    return 0; // Match does not exist
}

/*
 * Function: find_command_matches
 * ----------------------------
 * Finds commands that match the given prefix.
 *
 * Parameters:
 *   prefix - The prefix to match against command names.
 *   matches - The buffer to store the matching commands.
 *
 * Returns:
 *   The number of matching commands found.
 * 
 * Note:
 *  This function searches through the directories listed in the PATH environment variable.
 *  It should ignore PATH entries that start with "/mnt" or "/home" to avoid searching in those directories.
 */
static int find_command_matches(const char *prefix, char matches[][256])
{
    char *path = getenv("PATH");

    if (path == NULL)
        return 0; // No PATH environment variable found

    char *path_copy = strdup(path);

    if (path_copy == NULL)
        return 0; // Memory allocation failed

    int match_count = 0; // Count of matching commands found
    char *dir = strtok(path_copy, ":"); // Tokenize the PATH variable by ':'

    while (dir != NULL)
    {
        DIR *dp = opendir(dir);

        if (dp == NULL) // Unable to open directory, skip to the next one
        {
            dir = strtok(NULL, ":"); // Get the next directory in PATH
            continue; // Skip to the next directory if unable to open
        }

        struct dirent *entry;

        while ((entry = readdir(dp)) != NULL) // Read each entry in the directory
        {
            if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0) // Check if the entry matches the prefix
            {
                char full_path[PATH_MAX];

                snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name); // Construct the full path of the command

                if (access(full_path, X_OK) == 0) // Check if the command is executable
                {
                    // Skip duplicates
                    if (match_exists(matches, match_count, entry->d_name))
                        continue;

                    strcpy(matches[match_count], entry->d_name); // Copy the matching command to the output buffer
                    match_count++;

                    if (match_count >= MAX_MATCHES) // Check if the maximum number of matches has been reached
                        break; // Exit the loop if maximum matches reached
                }
            }
        }

        closedir(dp); // Close the directory stream

        dir = strtok(NULL, ":"); // Get the next directory in PATH
    }

    free(path_copy);

    qsort(matches, match_count, sizeof(matches[0]), compare_strings); // Sort the matches alphabetically

    return match_count; // Return the number of matching commands found
}

/*
 * Function: get_current_word
 * ----------------------------
 * Extracts the current word from the input buffer based on the cursor position.
 *
 * Parameters:
 *   buffer - The input buffer containing the line of text.
 *   cursor_position - The position of the cursor in the buffer.
 *   word - The buffer to store the extracted word.
 *   word_length - A pointer to store the length of the extracted word.
 */
static void get_current_word(const char *buffer, size_t cursor_position, char *word, size_t *word_length)
{
    // Find the start of the current word
    size_t start = cursor_position;
    while (start > 0 && buffer[start - 1] != ' ')
        start--;

    // Find the end of the current word
    size_t end = cursor_position;
    while (end < strlen(buffer) && buffer[end] != ' ')
        end++;

    // Copy the current word into the provided buffer
    *word_length = end - start;
    strncpy(word, buffer + start, *word_length);
    word[*word_length] = '\0'; // Null-terminate the string
}

/*
 * Function: longest_common_prefix
 * -------------------------------
 * Finds the longest common prefix among a list of strings.
 *
 * Parameters:
 *   matches - An array of strings to compare.
 *   count - The number of strings in the array.
 *
 * Returns:
 *   The length of the longest common prefix.
 */
static size_t longest_common_prefix(char matches[][256], int count)
{
    if (count == 0)
        return 0;

    size_t prefix_length = strlen(matches[0]);
    for (int i = 1; i < count; i++)
    {
        size_t j = 0;
        while (j < prefix_length && matches[0][j] && matches[i][j] && matches[i][j] == matches[0][j])
            j++;

        prefix_length = j;
    }

    return prefix_length;
}

/**
 * Function: print_matches
 * -----------------------
 * Prints the list of matching commands in a formatted grid.
 *
 * Parameters:
 *   matches - An array of strings representing the matching commands.
 *   match_count - The number of matching commands.
 */
static void print_matches(char matches[][256], int match_count)
{
    int columns = 0;
    
    // Get the terminal size
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        columns = 80; // Default to 80 columns if ioctl fails
    else
        columns = ws.ws_col; // Get the number of columns from the terminal size

    // Find the longest match width to format the output
    size_t max_width = 0;
    for (int i = 0; i < match_count; i++)
    {
        size_t width = strlen(matches[i]);
        if (width > max_width)
            max_width = width;
    }

    // Compute cell width
    size_t cell_width = max_width + 2; // Add padding for spacing

    // Compute number of columns that can fit in the terminal
    int num_columns = columns / cell_width;

    if (num_columns == 0) // Ensure at least one column is printed
        num_columns = 1;

    int rows = (match_count + num_columns - 1) / num_columns; // Calculate the number of rows needed

    printf("\n"); // Print a newline to separate the matches
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < num_columns; j++)
        {
            int index = i + j * rows; // Calculate the index of the match to print
            if (index < match_count) // Ensure the index is within bounds
            {
                printf("%-*s", (int)max_width, matches[index]); // Print each matching command with padding
                if (j < num_columns - 1) // Add spacing between columns (padding)
                    printf("  ");
            }
        }
        printf("\n"); // Print a newline after each row
    }

    if (match_count % num_columns != 0) // If the last row is not filled, print a newline
        printf("\n"); // Print a newline after the matches
}

/*
 * Function: complete_command
 * --------------------------
 * Completes the current command in the input buffer based on the cursor position.
 *
 * Parameters:
 *   buffer - The input buffer containing the line of text.
 *   length - Pointer to the current length of the input line.
 *   cursor_position - Pointer to the current cursor position in the input line.
 *   buffer_size - The size of the input buffer.
 */
static void complete_command(char *buffer, size_t *length, size_t *cursor_position, size_t buffer_size)
{
    char word[256]; // Buffer to hold the current word
    size_t word_length;

    get_current_word(buffer, *cursor_position, word, &word_length); // Extract the current word based on cursor position

    char matches[MAX_MATCHES][256]; // Buffer to hold the matching commands
    int match_count = find_command_matches(word, matches); // Find matching commands
    if (match_count == 0)
        return; // No matching commands found, exit the function

    if (previous_was_tab && strcmp(last_prefix, word) == 0) // If the previous key was a tab and the prefix hasn't changed
    {        
        print_matches(matches, match_count); // Print the matching commands in a formatted manner

        redraw_line(buffer, *length, *cursor_position); // Redraw the line with the updated buffer and cursor position
    }
    else // If this is the first tab press or the prefix has changed
    {
        size_t lcp = longest_common_prefix(matches, match_count); // Find the longest common prefix among the matches
        if (lcp > word_length) // If the longest common prefix is longer than the current word, insert the extra characters
        {
            size_t extra_length = lcp - word_length; // Calculate the length of the extra characters to insert
        
            memcpy(buffer + *cursor_position, matches[0] + word_length, extra_length); // Insert the matching command into the buffer
        
            *cursor_position += extra_length; // Update the cursor position
            *length += extra_length; // Update the length of the input line
        
            buffer[*length] = '\0'; // Null-terminate the string
        
            redraw_line(buffer, *length, *cursor_position); // Redraw the line with the updated buffer and cursor position
        }
        else // If the longest common prefix is not longer than the current word, just set the previous_was_tab flag and store the last prefix
        {
            previous_was_tab = 1; // Set the flag indicating that the previous key was a tab
            strncpy(last_prefix, word, sizeof(last_prefix)); // Store the current prefix for future reference
        }
    }
}

char *complete_line(const char *buffer, size_t cursor_position)
{
    static char word[256];
    size_t word_length;

    get_current_word(buffer, cursor_position, word, &word_length);

    // Placeholder for actual completion logic
    return word;
}

/*
 * Function: tab_complete
 * ----------------------
 * Handles tab completion for the input buffer.
 *
 * Parameters:
 *   buffer          - The input buffer containing the line of text.
 *   length          - Pointer to the current length of the input line.
 *   cursor_position - Pointer to the current cursor position in the input line.
 *   buffer_size     - Pointer to the size of the input buffer.
 */
void tab_complete(char *buffer, size_t *length, size_t *cursor_position, size_t *buffer_size)
{
    if (is_command_position(buffer, *cursor_position))
        complete_command(buffer, length, cursor_position, *buffer_size);
}

void completion_reset(void)
{
    previous_was_tab = 0; // Reset the flag for tab completion
    last_prefix[0] = '\0'; // Clear the last prefix buffer
}