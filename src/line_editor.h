#ifndef LINE_EDITOR_H
#define LINE_EDITOR_H

typedef struct LineEditor
{
    const char *prompt; // Pointer to the prompt string

    char *buffer;       // Pointer to the input buffer
    size_t buffer_size; // Size of the input buffer

    size_t length;      // Current length of the input line
    size_t cursor_position; // Current cursor position in the input line

    size_t prompt_start_col; // Column position where the prompt starts

    size_t prompt_length; // Visible length of the prompt string (excluding ANSI escape codes)
    size_t term_cols; // Number of terminal columns (width of the terminal)
    size_t last_cursor_row; // Last known cursor row position (0-indexed)
} LineEditor;

void redraw_line(LineEditor *ed);
char *line_editor_read(void);

#endif /* LINE_EDITOR_H */
