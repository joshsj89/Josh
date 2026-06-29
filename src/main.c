/*
 *      main.c
 *
 *      This file contains the main function of the shell program.
 */

#include "shell.h"

/**
 * Function: main
 * ----------------
 * The main function of the shell program.
 *
 * This function initializes the shell and starts the main loop.
 *
 * Returns:
 *   0 on successful completion.
 */
int main(void)
{
    shell_loop();

    return 0;
}
