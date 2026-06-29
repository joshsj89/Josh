/*
 *      line_editor.c
 *
 *      This file contains the implementation of the line editor functionality for the shell.
 *      This file is responsible for reading characters, cursor movement, escape sequences,
 *      and other line editing features.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "history.h"
#include "line_editor.h"

/* Structure to hold original terminal settings */
struct termios orig_termios;

/*
 * Function: enable_raw_mode
 * -------------------------
 * Enables raw mode for terminal input.
 *
 * This function modifies the terminal settings to disable canonical mode and echoing,
 * allowing for character-by-character input processing.
 */
void enable_raw_mode()
{
    // Save original terminal settings
    tcgetattr(STDIN_FILENO, &orig_termios);

    struct termios raw = orig_termios;
    // Disable canonical mode (line buffering) and echoing
    raw.c_lflag &= ~(ICANON | ECHO);

    // Apply new settings
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

/*
 * Function: disable_raw_mode
 * --------------------------
 * Disables raw mode for terminal input and restores original settings.
 *
 * This function restores the terminal settings to their original state,
 * re-enabling canonical mode and echoing.
 */
void disable_raw_mode()
{
    // Restore original settings before exiting
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

/*
 * Function: handle_up_down
 * ------------------------
 * Handles the up and down arrow key navigation for command history.
 *
 * This function retrieves the previous or next command from the history based on the user's input
 * and updates the input buffer accordingly. It also manages the cursor position and length of the input line.
 *
 * Parameters:
 *   next_command - The command to display (previous or next command from history).
 *   length       - Pointer to the current length of the input line.
 *   buffer       - The input buffer to update with the command.
 *   buffer_size  - The size of the input buffer.
 */
static void handle_up_down(const char *next_command, size_t *length, char *buffer, size_t buffer_size)
{
    if (next_command != NULL) // Check if there is a previous/next command
    {
        // Clear the current line
        while (*length > 0) // While there are characters in the current line
        {
            printf("\b \b"); // Move cursor back, print space, move cursor back again
            (*length)--; // Decrease the length of the input line
        }
        // Copy the previous/next command into the buffer
        snprintf(buffer, buffer_size, "%s", next_command); // Copy the previous/next command into the buffer
        *length = strlen(buffer); // Update the length of the input line
        printf("%s", buffer); // Print the previous/next command to the terminal
        fflush(stdout); // Flush the output buffer to ensure immediate display
    }
}

/*
 * Function: line_editor_read
 * ---------------------
 * Reads a line of input from the standard input.
 *
 * This function reads a line of input from the standard input and returns it as a dynamically allocated string.
 * It is responsible for allocating memory for the input string, reading characters, handling backspace and delete,
 * and ensuring that the input is null-terminated.
 *
 * Returns:
 *   A pointer to the dynamically allocated string containing the user input, or NULL on error or EOF.
 */
char *line_editor_read(void)
{
    // Allocate a buffer to hold the input line
    size_t buffer_size = 1024; // Initial buffer size
    char *buffer = malloc(buffer_size);
    if (buffer == NULL) // Check for memory allocation failure
    {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL; // Return NULL to indicate failure
    }

    // Enable raw mode for terminal input
    enable_raw_mode();

    // Read characters from standard input
    size_t length = 0; // Current length of the input line
    unsigned char c; // Variable to hold the character read from input
    while (read(STDIN_FILENO, &c, 1) == 1) // Read one character at a time
    {
        if (c == '\n') // Check for newline character (Enter key)
        {
            buffer[length] = '\0'; // Null-terminate the string
            break;                 // Exit the loop
        }
        else if (c == 127 || c == 8) // Check for delete or backspace character
        {
            if (length > 0) // Ensure there is something to delete
            {
                length--;              // Decrease the length of the input line
                buffer[length] = '\0'; // Null-terminate the string
                printf("\b \b");       // Move cursor back, print space, move cursor back again
                fflush(stdout);        // Flush the output buffer to ensure immediate display
            }
        }
        else if (c >= 32 && c <= 126) // Check for printable characters
        {
            if (length + 1 >= buffer_size) // Check if buffer needs to be resized
            {
                buffer_size *= 2;                                // Double the buffer size
                char *new_buffer = realloc(buffer, buffer_size); // Reallocate memory for the new buffer size
                if (new_buffer == NULL)                          // Check for memory allocation failure
                {
                    fprintf(stderr, "Memory allocation failed\n");
                    free(buffer); // Free the old buffer to prevent memory leak
                    return NULL;  // Return NULL to indicate failure
                }
                buffer = new_buffer; // Update the buffer pointer to the new buffer
            }
            
            buffer[length++] = c; // Add the character to the input line and increase length

            putchar(c); // Echo the character to the terminal
            fflush(stdout); // Flush the output buffer to ensure immediate display
        }
        else if (c == 4) // Check for Ctrl+D (EOF)
        {
            if (length == 0) // If the input line is empty, treat it as EOF
            {
                buffer = "exit";
                break;
            }
        }
        else if (c == 3) // Check for Ctrl+C (SIGINT)
        {
            free(buffer); // Free the allocated buffer
            return NULL; // Return NULL to indicate that the input was interrupted
        }
        else if (c == 27) // Check for escape character (start of escape sequence)
        {
            // Handle escape sequences (e.g., arrow keys)
            unsigned char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) break; // Read the next character
            if (read(STDIN_FILENO, &seq[1], 1) != 1) break; // Read the next character

            if (seq[0] == '[') // Check for CSI (Control Sequence Introducer)
            {
                switch (seq[1]) // Determine which arrow key was pressed
                {
                    case 'A': // Up arrow key
                        const char *prev_command = history_previous(); // Get the previous command from history
                        handle_up_down(prev_command, &length, buffer, buffer_size); // Handle the previous command
                        break;
                    case 'B': // Down arrow key
                        const char *next_command = history_next(); // Get the next command from history
                        handle_up_down(next_command, &length, buffer, buffer_size); // Handle the next command
                        break;
                    case 'C': // Right arrow key
                        break;
                    case 'D': // Left arrow key
                        break;
                    default:
                        break;
                }
            }
        }
    }

    // Disable raw mode for terminal input
    disable_raw_mode();

    // newline after the user presses Enter
    putchar('\n');

    return buffer; // Return the dynamically allocated input line
}
