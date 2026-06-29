#ifndef HISTORY_H
#define HISTORY_H

void history_init(void);
void history_add(const char *line);
void history_print(void);
void history_destroy(void);

const char *history_previous(void);
const char *history_next(void);

#endif /* HISTORY_H */