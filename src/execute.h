#ifndef EXECUTE_H
#define EXECUTE_H

#include "tokens.h"

void reap_background_jobs(void);
int execute_command(Command *cmd);

#endif /* EXECUTE_H */