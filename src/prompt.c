#include <limits.h> // For PATH_MAX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define COLOR_GREEN "\x1b[32m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_RESET "\x1b[0m"

/**
 * Function: get_prompt
 * ---------------------
 * Returns the shell prompt.
 * 
 * The prompt includes the username, session ID (if available), and the current working directory.
 * If the current working directory is within the user's home directory, it is displayed as '~'.
 * The prompt is color-coded for better visibility.
 * 
 * Returns:
 *   A pointer to a static string containing the formatted prompt.
 */
const char *get_prompt(void)
{
    static char prompt[PATH_MAX + 128];

    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        strcpy(cwd, "?");

    const char *user = getenv("USER");
    if (user == NULL)
        user = "unknown";

    const char *session = getenv("SESSION_ID");
    if (session == NULL)
        session = "";

    const char *home = getenv("HOME");

    char display_path[PATH_MAX];

    if (home && 
        strncmp(cwd, home, strlen(home)) == 0 && 
        (cwd[strlen(home)] == '/' || cwd[strlen(home)] == '\0')) // Check if cwd is home or a subdirectory of home
    {
        snprintf(display_path, sizeof(display_path), "~%s", cwd + strlen(home)); // Replace home directory with '~'
    }
    else
    {
        // snprintf(display_path, sizeof(display_path), "%s", cwd); // Use the full path if not in home directory
        strncpy(display_path, cwd, sizeof(display_path) - 1);
        display_path[sizeof(display_path) - 1] = '\0'; // Ensure null-termination because strncpy does not null-terminate if the source string is longer than the destination buffer
    }

    if (*session)
        snprintf(prompt, sizeof(prompt), COLOR_GREEN "%s-%s" COLOR_RESET ":" COLOR_BLUE "%s" COLOR_RESET "$ ", user, session, display_path);
    else
        snprintf(prompt, sizeof(prompt), COLOR_GREEN "%s" COLOR_RESET ":" COLOR_BLUE "%s" COLOR_RESET "$ ", user, display_path);
    
    return prompt;
}