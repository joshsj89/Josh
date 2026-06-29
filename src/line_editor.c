/*
 *      line_editor.c
 *
 *      This file contains the implementation of the line editor functionality for the shell.
 *      This file is responsible for reading characters, cursor movement, escape sequences,
 *      and other line editing features.
 *      
 *      TODO:
 *      - Implement tab completion
 *      - Implement forward delete (DEL key)
 *      - Implement Ctrl+Left/Right and Alt+B/F for word navigation
 *      - Implement Ctrl+U to clear the line
 *      - Implement Ctrl+K to delete from cursor to end of line
 *      - Implement Home and Ctrl+A to move cursor to the beginning of the line
 *      - Implement End and Ctrl+E to move cursor to the end of the line
 *      - Implement Ctrl+W to delete the word before the cursor
 *      - Implement Ctrl+Y to paste the last deleted text
 *      - Implement Ctrl+L to clear the screen and redraw the line
 *      - Implement Ctrl+R for reverse search in history
 *      - Implement Ctrl+T to transpose the character before the cursor with the character at the cursor
 *      - Implement Ctrl+H to delete the character before the cursor (backspace)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "history.h"
#include "line_editor.h"

// #define CTRL_KEY(k) ((k) & 0x1f) // Macro to convert a character to its corresponding control key

static const char *left_arrow = "\033[D";  // ANSI escape code for left arrow
static const char *right_arrow = "\033[C"; // ANSI escape code for right arrow

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
 * Function: redraw_line
 * ---------------------
 * Redraws the current input line with the updated buffer and cursor position.
 *
 * This function clears the current line, prints the prompt, the buffer content,
 * and moves the cursor to the correct position based on the cursor_position parameter.
 *
 * Parameters:
 *   buffer          - The input buffer containing the characters.
 *   length          - The current length of the input line.
 *   cursor_position - The current cursor position in the input line.
 */
static void redraw_line(const char *buffer, size_t length, size_t cursor_position)
{
    // Clear the current line
    printf("\r"); // Move cursor back to the beginning of the line
    printf("\033[2K"); // ANSI escape code to clear the entire line

    // Print the prompt
    printf("josh> ");

    // Print the buffer content
    fwrite(buffer, 1, length, stdout);

    // Move the cursor to the correct position
    for (size_t i = length; i > cursor_position; i--)
        printf("\b"); // Move cursor left to the desired position

    fflush(stdout); // Flush the output buffer to ensure immediate display
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
 *   cursor_position - Pointer to the current cursor position in the input line.
 *   buffer       - The input buffer to update with the command.
 *   buffer_size  - The size of the input buffer.
 */
static void handle_up_down(const char *next_command, size_t *length, size_t *cursor_position, char *buffer, size_t buffer_size)
{
    if (next_command != NULL) // Check if there is a previous/next command
    {
        // Copy the previous/next command into the buffer
        snprintf(buffer, buffer_size, "%s", next_command); // Copy the previous/next command into the buffer
        *length = strlen(buffer); // Update the length of the input line
        *cursor_position = *length; // Move the cursor to the end of the line

        redraw_line(buffer, *length, *cursor_position); // Redraw the line with the updated buffer and cursor position
    }
}

/*
 * Function: move_cursor_left
 * --------------------------
 * Moves the cursor one position to the left in the input line.
 *
 * This function checks if the cursor is not at the beginning of the line and moves it left if possible.
 *
 * Parameters:
 *   cursor_position - Pointer to the current cursor position in the input line.
 */
static void move_cursor_left(size_t *cursor_position)
{
    if (*cursor_position > 0) // Ensure the cursor is not at the beginning
    {
        printf("%s", left_arrow); // Move cursor left
        fflush(stdout); // Flush the output buffer to ensure immediate display
        (*cursor_position)--; // Decrease the cursor position
    }
}

/*
 * Function: move_cursor_right
 * ---------------------------
 * Moves the cursor one position to the right in the input line.
 *
 * This function checks if the cursor is not at the end of the line and moves it right if possible.
 *
 * Parameters:
 *   cursor_position - Pointer to the current cursor position in the input line.
 *   length          - The current length of the input line.
 *   buffer          - The input buffer containing the characters.
 */
static void move_cursor_right(size_t *cursor_position, size_t length)
{
    if (*cursor_position < length) // Ensure the cursor is not at the end
    {
        printf("%s", right_arrow); // Move cursor right
        fflush(stdout); // Flush the output buffer to ensure immediate display
        (*cursor_position)++; // Increase the cursor position
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
    size_t cursor_position = 0; // Current cursor position in the input line
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
            if (cursor_position > 0) // Ensure the cursor is not at the beginning of the line
            {
                memmove(buffer + cursor_position - 1, buffer + cursor_position, length - cursor_position + 1); // Shift characters after cursor to the left (+1 includes the null terminator)
                cursor_position--; // Move the cursor position to the left
                length--; // Decrease the length of the input line

                redraw_line(buffer, length, cursor_position); // Redraw the line with the updated buffer and cursor position
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
            
            memmove(buffer + cursor_position + 1, buffer + cursor_position, length - cursor_position); // Shift characters after cursor to the right
            buffer[cursor_position] = c; // Insert the new character at the cursor position
            cursor_position++; // Move the cursor position to the right
            length++; // Increase the length of the input line

            buffer[length] = '\0'; // Null-terminate the string

            redraw_line(buffer, length, cursor_position); // Redraw the line with the updated buffer and cursor position
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
                        handle_up_down(prev_command, &length, &cursor_position, buffer, buffer_size); // Handle the previous command
                        break;
                    case 'B': // Down arrow key
                        const char *next_command = history_next(); // Get the next command from history
                        handle_up_down(next_command, &length, &cursor_position, buffer, buffer_size); // Handle the next command
                        break;
                    case 'C': // Right arrow key
                        move_cursor_right(&cursor_position, length); // Move the cursor right
                        break;
                    case 'D': // Left arrow key
                        move_cursor_left(&cursor_position); // Move the cursor left
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
