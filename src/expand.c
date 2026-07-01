/*
 * expand.c
 *
 * This file contains the implementation of variable expansion logic.
 * 
 * TODO:
 *  - Implement support for more complex variable expansions, such as ${VAR} syntax.
 *  - Implement support for shell variables and special variables (e.g., $?, $!, etc.).
 *  - Implement support for command substitution (e.g., $(command) or `command`).
 *  - Implement support for arithmetic expansion (e.g., $((expression))).
 *  - Implement support for tilde expansion (e.g., ~username).
 *  - Implement support for escaping the '$' character (e.g., \$VAR should not expand).
 *  - Implement support for expanding variables in double-quoted strings while preserving spaces.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "expand.h"

/**
 * Function: append_string
 * -----------------------
 * Appends a string to the destination buffer.
 *
 * Parameters:
 *   dest - A pointer to the destination buffer.
 *   src - The source string to append.
 */
static void append_string(char **dest, const char *src)
{
    size_t src_len = strlen(src);
    memcpy(*dest, src, src_len);
    *dest += src_len;
}

/**
 * Function: expand_variable
 * -------------------------
 * Expands a single variable and returns its value.
 *
 * Parameters:
 *   input - A pointer to the input string containing the variable to expand.
 *
 * Returns:
 *   A pointer to the expanded variable value.
 */
static const char *expand_variable(const char **input)
{
    // Extract the variable name
    char var_name[256];
    size_t i = 0;
    while (**input != '\0' && **input != ' ' && **input != '$' && (isalnum(**input) || **input == '_') && i < sizeof(var_name) - 1)
        var_name[i++] = *(*input)++;
    var_name[i] = '\0';

    // Get the value of the environment variable
    const char *var_value = getenv(var_name);
    if (var_value == NULL)
        var_value = ""; // If the variable is not set, use an empty string

    return var_value;
}

/**
 * Function: expand_string
 * -----------------------
 * Expands variables in the input string and returns a new string with the expanded values.
 *
 * Parameters:
 *   input - The input string containing variables to expand.
 *
 * Returns:
 *   A newly allocated string with variables expanded. The caller is responsible for freeing this string.
 */
static char *expand_string(const char *input)
{
    char *result = malloc(strlen(input) + 1);
    size_t len = 0;
    char *ptr = result;
    while (*input != '\0')
    {
        if (*input != '$')
        {
            *ptr++ = *input++; // Move to the next character
            len++;
        }
        else
        {
            input++; // Move past the '$' character
            if (*input == '\0') // If the string ends with '$', return an empty string
                return "";

            if (isalpha(*input) || *input == '_') // Check if the next character is valid for a variable name
            {
                append_string(&ptr, expand_variable(&input)); // Append the variable value to the result
            }
            else if (*input == '{') // Handle special case for '${VAR}' syntax
            {
                input++; // Move past the '{' character
                append_string(&ptr, expand_variable(&input)); // Append the variable value to the result

                if (*input == '}') // Check for closing '}'
                {
                    input++; // Move past the '}' character
                }
                else
                {
                    fprintf(stderr, "Error: Missing closing '}' for variable expansion");
                    free(result);
                    return NULL; // Return NULL to indicate an error
                }
            }
            else if (*input == '$') // Handle special case for '$$' (PID of the shell)
            {
                snprintf(ptr, 20, "%d", getpid()); // Convert PID to string
                ptr += strlen(ptr); // Move the pointer forward
                input++; // Move past the second '$'
            }
        }
    }

    *ptr = '\0'; // Null-terminate the result string
    return result;
}

/**
 * Function: expand_variables
 * --------------------------
 * Expands variables in an array of strings.
 *
 * Parameters:
 *   tokens - An array of strings containing variables to expand.
 */
void expand_variables(char **tokens)
{
    for (int i = 0; tokens[i] != NULL; i++)
        tokens[i] = expand_string(tokens[i]);
}
