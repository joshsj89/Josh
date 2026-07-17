/*
 *      line_editor.c
 *
 *      This file contains the implementation of the line editor functionality for the shell.
 *      This file is responsible for reading characters, cursor movement, escape sequences,
 *      and other line editing features.
 *      
 *      TODO:
 *      - Implement Ctrl+Left/Right and Alt+B/F for word navigation
 *      - Implement Ctrl+Y to paste the last deleted text
 *      - Implement Ctrl+R for reverse search in history
 *      - Implement tab autocomplete for double quotes and single quotes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "completion.h"
#include "history.h"
#include "line_editor.h"
#include "prompt.h"

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
 * Function: ctrl_key
 * ------------------
 * Converts a character to its corresponding control key.
 *
 * Parameters:
 *   k - The character to convert.
 *
 * Returns:
 *   The corresponding control key.
 */
static inline int ctrl_key(int k)
{
    return k & 0x1f; // Convert a character to its corresponding control key
}

/*
 * Function: get_cursor_position
 * ----------------------------------
 * Checks if the cursor is at the first column of the terminal.
 * 
 * Parameters:
 *  row - Pointer to store the current row of the cursor.
 *  col - Pointer to store the current column of the cursor.
 *
 * Returns:
 *   0 if successful, -1 otherwise.
 * 
 * Note:
 *   This function sends an ANSI escape sequence to the terminal to query the cursor position.
 *   Therefore, it must be called in a context where the terminal is in raw mode and can respond to the query.
 */
int get_cursor_position(size_t *row, size_t *col)
{
    char buf[32];
    size_t i = 0;

    write(STDOUT_FILENO, "\033[6n", 4); // Request cursor position report

    while (i < sizeof(buf) - 1) // Read the response from the terminal
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            return -1; // Return -1 on read error
        if (buf[i] == 'R')
            break;
        i++;
    }

    buf[i + 1] = '\0';

    unsigned int row_temp, col_temp;
    if (sscanf(buf, "\033[%u;%uR", &row_temp, &col_temp) != 2)
        return -1;

    *row = row_temp;
    *col = col_temp;

    return 0;
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
 *   ed - Pointer to the LineEditor instance containing the buffer, prompt, and cursor position.
 */
void redraw_line(LineEditor *ed)
{
    // Update terminal width dynamically in case the terminal size changes during input
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
        ed->term_cols = w.ws_col;
    size_t cols = ed->term_cols;

    // Move cursor up by the number of rows the input line occupies
    if (ed->last_cursor_row > 0)
        printf("\033[%zuA", ed->last_cursor_row);

    // Move cursor to prompt start column
    printf("\r"); // Move cursor back to the beginning of the line
    if (ed->prompt_start_col > 1)
        printf("\033[%zuC", ed->prompt_start_col - 1); // Move cursor to the prompt's start column

    printf("\033[J"); // ANSI escape code to clear from cursor to the bottom of the screen

    // Print the prompt
    fputs(ed->prompt, stdout);

    // Print the buffer content
    fwrite(ed->buffer, 1, ed->length, stdout);

    // Calculate the new cursor position in terms of rows and columns
    size_t cursor_idx = (ed->prompt_start_col - 1) + ed->prompt_length + ed->cursor_position;
    size_t target_row = cursor_idx / cols;
    size_t target_col = cursor_idx % cols;

    size_t total_idx = (ed->prompt_start_col - 1) + ed->prompt_length + ed->length;
    size_t total_rows = total_idx / cols;

    // Move cursor to the correct row
    if (total_rows > target_row)
        printf("\033[%zuA", total_rows - target_row); // Move cursor up to the target row
    printf("\033[%zuG", target_col + 1); // Move cursor to the target column (1-indexed)

    ed->last_cursor_row = target_row; // Update the last known cursor row position

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
 *   ed - Pointer to the LineEditor instance containing the buffer, prompt, and cursor position.
 *   next_command - The command to display (previous or next command from history).
 *
 */
static void handle_up_down(LineEditor *ed, const char *next_command)
{
    if (next_command != NULL) // Check if there is a previous/next command
    {
        // Copy the previous/next command into the buffer
        snprintf(ed->buffer, ed->buffer_size, "%s", next_command); // Copy the previous/next command into the buffer
        ed->length = strlen(ed->buffer); // Update the length of the input line
        ed->cursor_position = ed->length; // Move the cursor to the end of the line

        redraw_line(ed); // Redraw the line with the updated buffer and cursor position
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
 *   ed - Pointer to the LineEditor instance containing the buffer, prompt, and cursor position.
 */
static void move_cursor_left(LineEditor *ed)
{
    if (ed->cursor_position > 0) // Ensure the cursor is not at the beginning
    {
        printf("%s", left_arrow); // Move cursor left
        fflush(stdout); // Flush the output buffer to ensure immediate display
        ed->cursor_position--; // Decrease the cursor position
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
 *   ed - Pointer to the LineEditor instance containing the buffer, prompt, and cursor position.
 *   length          - The current length of the input line.
 *
 */
static void move_cursor_right(LineEditor *ed, size_t length)
{
    if (ed->cursor_position < length) // Ensure the cursor is not at the end
    {
        printf("%s", right_arrow); // Move cursor right
        fflush(stdout); // Flush the output buffer to ensure immediate display
        ed->cursor_position++; // Increase the cursor position
    }
}

/*
 * Function: delete_backward
 * -------------------------
 * Deletes the character before the cursor in the input line.
 *
 * This function checks if the cursor is not at the beginning of the line and deletes the character before it if possible.
 *
 * Parameters:
 *   ed - Pointer to the LineEditor instance containing the buffer, prompt, and cursor position.
 */
static void delete_backward(LineEditor *ed)
{
    if (ed->cursor_position > 0) // Ensure the cursor is not at the beginning of the line
    {
        memmove(ed->buffer + ed->cursor_position - 1, ed->buffer + ed->cursor_position, ed->length - ed->cursor_position + 1); // Shift characters after cursor to the left (+1 includes the null terminator)
        ed->cursor_position--; // Move the cursor position to the left
        ed->length--; // Decrease the length of the input line

        redraw_line(ed); // Redraw the line with the updated buffer and cursor position
        completion_reset(); // Reset the completion state
    }
}

/*
 * Function: delete_forward
 * -------------------------
 * Deletes the character after the cursor in the input line.
 *
 * This function checks if the cursor is not at the end of the line and deletes the character after it if possible.
 *
 * Parameters:
 *   ed - Pointer to the LineEditor instance containing the buffer, prompt, and cursor position.
 */
static void delete_forward(LineEditor *ed)
{
    if (ed->cursor_position < ed->length) // Ensure the cursor is not at the end of the line
    {
        memmove(ed->buffer + ed->cursor_position, ed->buffer + ed->cursor_position + 1, ed->length - ed->cursor_position); // Shift characters after cursor to the left
        ed->length--; // Decrease the length of the input line

        redraw_line(ed); // Redraw the line with the updated buffer and cursor position
        completion_reset(); // Reset the completion state
    }
}

/*
 * Function: insert_character
 * --------------------------
 * Inserts a character at the current cursor position in the input line.
 *
 * This function checks if the buffer needs to be resized, shifts characters after the cursor to the right,
 * inserts the new character, and updates the cursor position and length of the input line.
 *
 * Parameters:
 *   ed - Pointer to the LineEditor instance containing the buffer, prompt, and cursor position.
 *   c - The character to insert at the cursor position.
 */
static void insert_character(LineEditor *ed, unsigned char c)
{
    if (ed->length + 1 >= ed->buffer_size) // Check if buffer needs to be resized
    {
        ed->buffer_size *= 2; // Double the buffer size
        char *new_buffer = realloc(ed->buffer, ed->buffer_size); // Reallocate memory for the new buffer size
        if (new_buffer == NULL) // Check for memory allocation failure
        {
            fprintf(stderr, "Memory allocation failed\n");
            free(ed->buffer); // Free the old buffer to prevent memory leak
            return; // Return to indicate failure
        }
        ed->buffer = new_buffer; // Update the buffer pointer to the new buffer
    }

    memmove(ed->buffer + ed->cursor_position + 1, ed->buffer + ed->cursor_position, ed->length - ed->cursor_position); // Shift characters after cursor to the right
    ed->buffer[ed->cursor_position] = c; // Insert the new character at the cursor position
    ed->cursor_position++;          // Move the cursor position to the right
    ed->length++;                   // Increase the length of the input line

    ed->buffer[ed->length] = '\0'; // Null-terminate the string

    redraw_line(ed); // Redraw the line with the updated buffer and cursor position
    completion_reset(); // Reset the completion state
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
    LineEditor ed = {0};

    // Allocate a buffer to hold the input line
    ed.buffer_size = 1024; // Initial buffer size
    ed.buffer = malloc(ed.buffer_size);
    if (ed.buffer == NULL) // Check for memory allocation failure
    {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL; // Return NULL to indicate failure
    }

    ed.prompt = get_prompt(); // Get the current prompt string

    // Enable raw mode for terminal input
    enable_raw_mode();

    // Get current terminal width
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    ed.term_cols = w.ws_col > 0 ? w.ws_col : 80; // Store the number of terminal columns

    // Query cursor position before printing prompt
    size_t row, col;
    if (get_cursor_position(&row, &col) == -1)
    {
        fprintf(stderr, "Failed to get cursor position\n");
        free(ed.buffer); // Free the allocated buffer to prevent memory leak
        disable_raw_mode(); // Disable raw mode before returning
        return NULL; // Return NULL to indicate failure
    }

    ed.prompt_start_col = col; // Track where the prompt begins

    fputs(ed.prompt, stdout);
    fflush(stdout);

    // Query cursor position after printing prompt
    size_t after_row, after_col;
    if (get_cursor_position(&after_row, &after_col) == 0)
    {
        if (after_row > row)
            ed.prompt_length = (ed.term_cols - col + 1) + (after_row - row - 1) * ed.term_cols + (after_col - 1);
        else
            ed.prompt_length = after_col - col;
    }
    else
    {
        ed.prompt_length = 15; // Fallback length
    }

    // Read characters from standard input
    ed.length = 0; // Current length of the input line
    ed.cursor_position = 0; // Current cursor position in the input line
    unsigned char c; // Variable to hold the character read from input
    while (read(STDIN_FILENO, &c, 1) == 1) // Read one character at a time
    {
        if (c == '\n') // Check for newline character (Enter key)
        {
            ed.buffer[ed.length] = '\0'; // Null-terminate the string
            break;                 // Exit the loop
        }
        else if (c == 127 || c == 8 || c == ctrl_key('h')) // Check for delete or backspace character
        {
            delete_backward(&ed);
        }
        else if (c >= 32 && c <= 126) // Check for printable characters
        {
            insert_character(&ed, c); // Insert the character into the buffer
        }
        else if (c == ctrl_key('d')) // Check for Ctrl+D (EOF)
        {
            if (ed.length == 0) // If the input line is empty, treat it as EOF
            {
                strcpy(ed.buffer, "exit"); // Set buffer to "exit" to signal shell exit
                break;
            }
            else // If the input line is not empty, forward delete
            {
                delete_forward(&ed); // Delete the character at the cursor position
            }
        }
        else if (c == ctrl_key('c')) // Check for Ctrl+C (SIGINT)
        {
            free(ed.buffer); // Free the allocated buffer
            return NULL; // Return NULL to indicate that the input was interrupted
        }
        else if (c == '\t') // Check for tab character (tab completion) and Ctrl+I
        {
            tab_complete(&ed); // Handle tab completion
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
                        handle_up_down(&ed, prev_command); // Handle the previous command
                        completion_reset(); // Reset the completion state
                        break;
                    case 'B': // Down arrow key
                        const char *next_command = history_next(); // Get the next command from history
                        handle_up_down(&ed, next_command); // Handle the next command
                        completion_reset(); // Reset the completion state
                        break;
                    case 'C': // Right arrow key
                        move_cursor_right(&ed, ed.length); // Move the cursor right
                        completion_reset(); // Reset the completion state
                        break;
                    case 'D': // Left arrow key
                        move_cursor_left(&ed); // Move the cursor left
                        completion_reset(); // Reset the completion state
                        break;
                    case 'F': // End key
                        ed.cursor_position = ed.length; // Move cursor to the end of the line
                        redraw_line(&ed); // Redraw the line with the updated cursor position
                        completion_reset(); // Reset the completion state
                        break;
                    case 'H': // Home key
                        ed.cursor_position = 0; // Move cursor to the beginning of the line
                        redraw_line(&ed); // Redraw the line with the updated cursor position
                        completion_reset(); // Reset the completion state
                        break;
                    case '3': // Delete key (ESC [ 3 ~)
                        unsigned char tilde;
                        if (read(STDIN_FILENO, &tilde, 1) != 1) break; // Read the next character
                        if (tilde == '~') // Check for the tilde character
                            delete_forward(&ed); // Delete the character at the cursor position
                        break;
                    default:
                        break;
                }
            }
        }
        else if (c == ctrl_key('a')) // Check for Ctrl+A (move cursor to beginning of line)
        {
            ed.cursor_position = 0; // Move cursor to the beginning of the line
            redraw_line(&ed); // Redraw the line with the updated cursor position
            completion_reset(); // Reset the completion state
        }
        else if (c == ctrl_key('e')) // Check for Ctrl+E (move cursor to end of line)
        {
            ed.cursor_position = ed.length; // Move cursor to the end of the line
            redraw_line(&ed); // Redraw the line with the updated cursor position
            completion_reset(); // Reset the completion state
        }
        else if (c == ctrl_key('l')) // Check for Ctrl+L (clear screen)
        {
            printf("\033[H\033[J"); // ANSI escape code to clear the screen and move cursor to home position
            redraw_line(&ed); // Redraw the line with the updated cursor position
            completion_reset(); // Reset the completion state
        }
        else if (c == ctrl_key('u')) // Check for Ctrl+U (clear line)
        {
            ed.length = 0; // Clear the input line
            ed.cursor_position = 0; // Move cursor to the beginning of the line
            ed.buffer[0] = '\0'; // Null-terminate the string
            redraw_line(&ed); // Redraw the line with the updated buffer and cursor position
            completion_reset(); // Reset the completion state
        }
        else if (c == ctrl_key('k')) // Check for Ctrl+K (delete from cursor to end of line)
        {
            ed.buffer[ed.cursor_position] = '\0'; // Null-terminate the string at the cursor position
            ed.length = ed.cursor_position; // Update the length of the input line
            redraw_line(&ed); // Redraw the line with the updated buffer and cursor position
            completion_reset(); // Reset the completion state
        }
        else if (c == ctrl_key('b')) // Check for Ctrl+B (move cursor left)
        {
            move_cursor_left(&ed); // Move the cursor left
            completion_reset(); // Reset the completion state
        }
        else if (c == ctrl_key('f')) // Check for Ctrl+F (move cursor right)
        {
            move_cursor_right(&ed, ed.length); // Move the cursor right
            completion_reset(); // Reset the completion state
        }
        else if (c == ctrl_key('p')) // Check for Ctrl+P (previous command in history)
        {
            const char *prev_command = history_previous(); // Get the previous command from history
            handle_up_down(&ed, prev_command); // Handle the previous command
            completion_reset(); // Reset the completion state
        }
        else if (c == ctrl_key('n')) // Check for Ctrl+N (next command in history)
        {
            const char *next_command = history_next(); // Get the next command from history
            handle_up_down(&ed, next_command); // Handle the next command
            completion_reset(); // Reset the completion state
        }
        else if (c == ctrl_key('w')) // Check for Ctrl+W (delete word before cursor)
        {
            if (ed.cursor_position > 0) // Ensure there is a word to delete
            {
                size_t new_cursor_position = ed.cursor_position;
                while (new_cursor_position > 0 && ed.buffer[new_cursor_position - 1] == ' ') // Skip spaces
                    new_cursor_position--;
                
                while (new_cursor_position > 0 && ed.buffer[new_cursor_position - 1] != ' ') // Find start of word
                    new_cursor_position--;

                memmove(ed.buffer + new_cursor_position, ed.buffer + ed.cursor_position, ed.length - ed.cursor_position + 1); // Shift characters after cursor to the left (+1 includes null terminator)
                ed.length -= (ed.cursor_position - new_cursor_position); // Update length of input line
                ed.cursor_position = new_cursor_position; // Update cursor position

                redraw_line(&ed); // Redraw the line with the updated buffer and cursor position
            }
        }
        else if (c == ctrl_key('t')) // Check for Ctrl+T (transpose characters)
        {
            if (ed.cursor_position > 0 && ed.cursor_position < ed.length) // Ensure there are characters to transpose
            {
                char temp = ed.buffer[ed.cursor_position - 1]; // Store the character before the cursor
                ed.buffer[ed.cursor_position - 1] = ed.buffer[ed.cursor_position]; // Swap with the character at the cursor
                ed.buffer[ed.cursor_position] = temp; // Complete the swap

                ed.cursor_position++; // Move the cursor to the right after transposing
                redraw_line(&ed); // Redraw the line with the updated buffer and cursor position
            }
            else if (ed.cursor_position == ed.length && ed.length > 1) // If cursor is at the end, transpose the last two characters
            {
                char temp = ed.buffer[ed.length - 2]; // Store the second last character
                ed.buffer[ed.length - 2] = ed.buffer[ed.length - 1]; // Swap with the last character
                ed.buffer[ed.length - 1] = temp; // Complete the swap

                redraw_line(&ed); // Redraw the line with the updated buffer and cursor position
            }
        }
        // else if (c == ctrl_key('r')) // Check for Ctrl+R (reverse search in history)
        // {
        //     const char *search_result = history_reverse_search(); // Get the result of the reverse search from history
        //     if (search_result != NULL) // Check if a matching command was found
        //     {
        //         snprintf(ed.buffer, ed.buffer_size, "%s", search_result); // Copy the matching command into the buffer
        //         ed.length = strlen(ed.buffer); // Update the length of the input line
        //         ed.cursor_position = ed.length; // Move cursor to the end of the line

        //         redraw_line(&ed); // Redraw the line with the updated buffer and cursor position
        //     }
        // }
        // else if (c == ctrl_key('y')) // Check for Ctrl+Y (paste last deleted text)
        // {
        //     const char *last_deleted = history_last_deleted(); // Get the last deleted text from history
        //     if (last_deleted != NULL) // Check if there is any deleted text to paste
        //     {
        //         size_t last_deleted_length = strlen(last_deleted); // Get the length of the last deleted text
        //         if (ed.length + last_deleted_length >= buffer_size) // Check if buffer needs to be resized
        //         {
        //             buffer_size = ed.length + last_deleted_length + 1; // Update buffer size to accommodate the new text
        //             char *new_buffer = realloc(ed.buffer, buffer_size); // Reallocate memory
        //             if (new_buffer == NULL) // Check for memory allocation failure
        //             {
        //                 fprintf(stderr, "Memory allocation failed\n");
        //                 free(ed.buffer); // Free the old buffer to prevent memory leak
        //                 return NULL; // Return NULL to indicate failure
        //             }
        //             ed.buffer = new_buffer; // Update the buffer pointer to the new buffer
        //         }

        //         memmove(ed.buffer + ed.cursor_position + last_deleted_length, ed.buffer + ed.cursor_position, ed.length - ed.cursor_position + 1); // Shift characters after cursor to the right (+1 includes null terminator)
        //         memcpy(ed.buffer + ed.cursor_position, last_deleted, last_deleted_length); // Insert the last deleted text at the cursor position
        //         ed.length += last_deleted_length; // Update length of input line
        //         ed.cursor_position += last_deleted_length; // Move cursor position to the right after pasting

        //         redraw_line(&ed); // Redraw the line with the updated buffer and cursor position
        //     }
        // }
    }

    // Disable raw mode for terminal input
    disable_raw_mode();

    // newline after the user presses Enter
    putchar('\n');

    return ed.buffer; // Return the dynamically allocated input line
}
