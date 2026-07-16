/*
 *      File: completion.c
 *      -------------------
 *      This file is intended to handle tab completion functionality for the shell.
 *
 */

#include <dirent.h> // For directory operations
#include <limits.h> // For PATH_MAX
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
    // Reorder "./" to be in front of "../"
    if (strcmp((const char *)a, "./") == 0)
        return -1; // "./" should come before any other string
    if (strcmp((const char *)b, "./") == 0)
        return 1; // Any other string should come after "./"

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
        if (buffer[i] == ' ')
            return 0; // Not at the command position

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
        if (strcmp(matches[i], new_match) == 0)
            return 1; // Match already exists

    return 0; // Match does not exist
}

/*
 * Function: add_match
 * -------------------
 * Adds a new match to the list of matches.
 *
 * Parameters:
 *   matches - The buffer containing the list of matches.
 *   match_count - A pointer to the count of matches.
 *   new_match - The match to add.
 *
 * Returns:
 *   1 if the match was added successfully, 0 otherwise.
 */
static int add_match(char matches[][256], int *match_count, const char *new_match)
{
    // Cannot add more matches, buffer is full
    if (*match_count >= MAX_MATCHES)
        return 0;

    // Skip duplicates
    if (match_exists(matches, *match_count, new_match))
        return 0;

    strcpy(matches[*match_count], new_match); // Copy the new match into the matches buffer
    (*match_count)++;

    return 1; // Match added successfully
}

/*
 * Function: find_command_matches
 * ----------------------------
 * Finds commands that match the given prefix.
 *
 * Parameters:
 *   prefix - The prefix to match against command names.
 *   matches - The buffer to store the matching commands.
 *   word_length - A pointer to store the length of the word being completed.
 *
 * Returns:
 *   The number of matching commands found.
 * 
 * Note:
 *  This function searches through the directories listed in the PATH environment variable.
 *  It should ignore PATH entries that start with "/mnt" or "/home" to avoid searching in those directories.
 */
static int find_command_matches(const char *prefix, char matches[][256], size_t *word_length)
{
    (void)word_length; // Suppress unused parameter warning
    
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
                    add_match(matches, &match_count, entry->d_name); // Add the matching command to the output buffer
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
 * Function: split_prefix
 * ----------------------
 * Splits a prefix into a directory part and a file prefix.
 *
 * Parameters:
 *   prefix - The prefix to split.
 *   dir - An array to store the directory part.
 *   file_prefix - An array to store the file prefix.
 *   word_length - A pointer to store the length of the word being completed.
 */
static void split_prefix(const char *prefix, char *dir, char *file_prefix, size_t *word_length)
{
    const char *last_slash = strrchr(prefix, '/'); // Find the last occurrence of '/' in the prefix
    
    if (last_slash)
    {
        size_t dir_length = last_slash - prefix; // Calculate the length of the directory part

        if (dir_length == 0) // If the prefix is just "/", set dir to "/" and file_prefix to empty
            strcpy(dir, "/");
        else
        {
            strncpy(dir, prefix, dir_length); // Copy the directory part into dir
            dir[dir_length] = '\0';
        }

        strncpy(file_prefix, last_slash + 1, sizeof(file_prefix) - 1);
        *word_length = strlen(file_prefix); // Store the length of the file prefix
    }
    else // If there is no '/', set dir to current directory and file_prefix to the entire prefix
    {
        strcpy(dir, ".");
        strcpy(file_prefix, prefix);
        *word_length = strlen(file_prefix); // Store the length of the file prefix
    }
}

/*
 * Function: find_filename_matches
 * -------------------------------
 * Finds all filenames in the current directory that match the given prefix.
 *
 * Parameters:
 *   prefix - The prefix to match filenames against.
 *   matches - An array to store the matching filenames.
 *   word_length - A pointer to store the length of the word being completed.
 *
 * Returns:
 *   The number of matching filenames found.
 */
static int find_filename_matches(const char *prefix, char matches[][256], size_t *word_length)
{
    char dir[PATH_MAX];
    char file_prefix[NAME_MAX];

    split_prefix(prefix, dir, file_prefix, word_length); // Split the prefix into directory and file prefix

    DIR *dp = opendir(dir); // Open the current directory

    if (dp == NULL)
        return 0; // Unable to open directory

    int match_count = 0; // Count of matching filenames found
    struct dirent *entry;

    while ((entry = readdir(dp)) != NULL) // Read each entry in the directory
    {
        int want_hidden = (file_prefix[0] == '.'); // Determine if hidden files should be included based on the prefix

        if (!want_hidden) // Skip '.' and '..' entries if not looking for hidden files
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

        if (strncmp(entry->d_name, file_prefix, strlen(file_prefix)) == 0) // Check if the entry matches the prefix
        {
            char file_name[256];
            strncpy(file_name, entry->d_name, sizeof(file_name) - 1);
            file_name[sizeof(file_name) - 1] = '\0';

            if (entry->d_type == DT_DIR) // Check if the entry is a directory
                strncat(file_name, "/", sizeof(file_name) - strlen(file_name) - 1); // Append '/' to directory names

            add_match(matches, &match_count, file_name); // Add the matching filename to the output buffer
        }
    }

    closedir(dp); // Close the directory stream

    qsort(matches, match_count, sizeof(matches[0]), compare_strings); // Sort the matches alphabetically

    return match_count; // Return the number of matching filenames found
}

/*
 * Function: find_executable_matches
 * ---------------------------------
 * Finds all executable files in the specified directory that match the given prefix.
 *
 * Parameters:
 *   prefix       - The prefix to match against filenames.
 *   matches      - An array to store the matching filenames.
 *   word_length  - A pointer to store the length of the file prefix.
 *
 * Returns:
 *   The number of matching filenames found.
 */
static int find_executable_matches(const char *prefix, char matches[][256], size_t *word_length)
{
    char dir[PATH_MAX];
    char file_prefix[NAME_MAX];

    split_prefix(prefix, dir, file_prefix, word_length); // Split the prefix into directory and file prefix

    DIR *dp = opendir(dir); // Open the current directory

    if (dp == NULL)
        return 0; // Unable to open directory

    int match_count = 0; // Count of matching filenames found
    struct dirent *entry;

    while ((entry = readdir(dp)) != NULL) // Read each entry in the directory
    {
        int want_hidden = (file_prefix[0] == '.'); // Determine if hidden files should be included based on the prefix

        if (!want_hidden) // Skip '.' and '..' entries if not looking for hidden files
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

        if (strncmp(entry->d_name, file_prefix, strlen(file_prefix)) == 0) // Check if the entry matches the prefix
        {
            char file_name[256];
            strncpy(file_name, entry->d_name, sizeof(file_name) - 1);
            file_name[sizeof(file_name) - 1] = '\0';

            if (entry->d_type == DT_DIR) // Check if the entry is a directory
                strncat(file_name, "/", sizeof(file_name) - strlen(file_name) - 1); // Append '/' to directory names
            else if (access(file_name, X_OK) != 0) // Check if the entry is executable
                continue; // Skip non-executable files

            add_match(matches, &match_count, file_name); // Add the matching filename to the output buffer
        }
    }

    closedir(dp); // Close the directory stream

    qsort(matches, match_count, sizeof(matches[0]), compare_strings); // Sort the matches alphabetically

    return match_count; // Return the number of matching filenames found
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
 * Function: complete_matches
 * --------------------------
 * Completes the current word in the input buffer based on the cursor position.
 *
 * Parameters:
 *   ed - Pointer to the LineEditor instance containing the buffer, prompt, and cursor position.
 *   match_func - Pointer to the function used to find matching commands/filenames.
 */
static void complete_matches(LineEditor *ed, int (*match_func)(const char *, char [][256], size_t *))
{
    char word[256]; // Buffer to hold the current word
    size_t word_length;

    get_current_word(ed->buffer, ed->cursor_position, word, &word_length); // Extract the current word based on cursor position

    char matches[MAX_MATCHES][256]; // Buffer to hold the matching words
    int match_count = match_func(word, matches, &word_length); // Find matching words
    if (match_count == 0)
        return; // No matching words found, exit the function

    if (previous_was_tab && strcmp(last_prefix, word) == 0) // If the previous key was a tab and the prefix hasn't changed
    {        
        print_matches(matches, match_count); // Print the matching words in a formatted manner

        redraw_line(ed); // Redraw the line with the updated buffer and cursor position
    }
    else // If this is the first tab press or the prefix has changed
    {
        size_t lcp = longest_common_prefix(matches, match_count); // Find the longest common prefix among the matches
        if (lcp > word_length) // If the longest common prefix is longer than the current word, insert the extra characters
        {
            size_t extra_length = lcp - word_length; // Calculate the length of the extra characters to insert
        
            memcpy(ed->buffer + ed->cursor_position, matches[0] + word_length, extra_length); // Insert the matching command into the buffer
        
            ed->cursor_position += extra_length; // Update the cursor position
            ed->length += extra_length; // Update the length of the input line
        
            ed->buffer[ed->length] = '\0'; // Null-terminate the string
        
            redraw_line(ed); // Redraw the line with the updated buffer and cursor position
        }
        else // If the longest common prefix is not longer than the current word, just set the previous_was_tab flag and store the last prefix
        {
            previous_was_tab = 1; // Set the flag indicating that the previous key was a tab
            strncpy(last_prefix, word, sizeof(last_prefix)); // Store the current prefix for future reference
        }
    }
}

/*
 * Function: complete_line
 * -----------------------
 * Completes the current line based on the cursor position and returns the completed word.
 *
 * Parameters:
 *   buffer - The input buffer containing the line of text.
 *   cursor_position - The position of the cursor in the buffer.
 *
 * Returns:
 *   A pointer to the completed word, or NULL if no completion is possible.
 */
static char *complete_line(const char *buffer, size_t cursor_position)
{
    static char word[256];
    size_t word_length;

    get_current_word(buffer, cursor_position, word, &word_length);

    return word;
}

/*
 * Function: tab_complete
 * ----------------------
 * Handles tab completion for the input buffer.
 *
 * Parameters:
 *   ed - Pointer to the LineEditor instance containing the buffer, prompt, and cursor position.
 */
void tab_complete(LineEditor *ed)
{
    char *current_word = complete_line(ed->buffer, ed->cursor_position); // Get the current word based on cursor position

    if (is_command_position(ed->buffer, ed->cursor_position))
    {
        if (strchr(current_word, '/') != NULL) // If the current word contains a '/', look for executables
            complete_matches(ed, find_executable_matches);
        else // Otherwise, treat it as a command
            complete_matches(ed, find_command_matches);
    }
    else
        complete_matches(ed, find_filename_matches);
}

/**
 * Function: completion_reset
 * --------------------------
 * Resets the completion state.
 */
void completion_reset(void)
{
    previous_was_tab = 0; // Reset the flag for tab completion
    last_prefix[0] = '\0'; // Clear the last prefix buffer
}
