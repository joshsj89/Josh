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
#include <unistd.h> // For access()
#include "completion.h"

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
 * Function: find_command_match
 * ----------------------------
 * Finds a command that matches the given prefix.
 *
 * Parameters:
 *   prefix - The prefix to match against command names.
 *   match - The buffer to store the matching command.
 *   match_size - The size of the match buffer.
 *
 * Returns:
 *   1 if a matching command is found, 0 otherwise.
 * 
 * Note:
 *  This function searches through the directories listed in the PATH environment variable.
 *  It should ignore PATH entries that start with "/mnt" or "/home" to avoid searching in those directories.
 */
static int find_command_match(const char *prefix, char *match, size_t match_size)
{
    char *path = getenv("PATH");

    if (path == NULL)
        return 0; // No PATH environment variable found

    char *path_copy = strdup(path);

    if (path_copy == NULL)
        return 0; // Memory allocation failed

    char *dir = strtok(path_copy, ":"); // Tokenize the PATH variable by ':'

    while (dir != NULL)
    {
        if (strncmp(dir, "/mnt", 4) == 0 || strncmp(dir, "/home", 5) == 0) // Ignore directories starting with "/mnt" or "/home"
        {
            dir = strtok(NULL, ":"); // Get the next directory in PATH
            continue; // Skip to the next directory
        }
        
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
                    strncpy(match, entry->d_name, match_size); // Copy the matching command to the output buffer
                    match[match_size - 1] = '\0'; // Ensure null-termination

                    closedir(dp); // Close the directory stream
                    free(path_copy); // Free the duplicated PATH string
                    
                    return 1; // Found a matching command
                }
            }
        }

        closedir(dp); // Close the directory stream

        dir = strtok(NULL, ":"); // Get the next directory in PATH
    }

    free(path_copy);

    return 0; // No matching command found
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
    {
        start--;
    }

    // Find the end of the current word
    size_t end = cursor_position;
    while (end < strlen(buffer) && buffer[end] != ' ')
    {
        end++;
    }

    // Copy the current word into the provided buffer
    *word_length = end - start;
    strncpy(word, buffer + start, *word_length);
    word[*word_length] = '\0'; // Null-terminate the string
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

    char match[256]; // Buffer to hold the matching command
    if (!find_command_match(word, match, sizeof(match))) // Find a matching command
        return; // No matching command found, exit the function

    size_t extra_length = strlen(match) - word_length; // Calculate the length of the extra characters to insert

    memcpy(buffer + *cursor_position, match + word_length, extra_length); // Insert the matching command into the buffer

    *cursor_position += extra_length; // Update the cursor position
    *length += extra_length; // Update the length of the input line

    buffer[*length] = '\0'; // Null-terminate the string

    redraw_line(buffer, *length, *cursor_position); // Redraw the line with the updated buffer and cursor position
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
    {
        complete_command(buffer, length, cursor_position, *buffer_size);
    }
}