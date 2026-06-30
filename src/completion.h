#ifndef COMPLETION_H
#define COMPLETION_H

#include <stddef.h> // For size_t

void redraw_line(const char *buffer, size_t length, size_t cursor_position);

char *complete_line(const char *buffer, size_t cursor_position);
void tab_complete(char *buffer, size_t *length, size_t *cursor_position);
void completion_reset(void);

#endif /* COMPLETION_H */