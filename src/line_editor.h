#ifndef LINE_EDITOR_H
#define LINE_EDITOR_H

void redraw_line(const char *buffer, size_t length, size_t cursor_position);
char *line_editor_read(void);

#endif /* LINE_EDITOR_H */