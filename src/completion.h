#ifndef COMPLETION_H
#define COMPLETION_H

#include <stddef.h> // For size_t
#include "line_editor.h"

void tab_complete(LineEditor *ed);
void completion_reset(void);

#endif /* COMPLETION_H */