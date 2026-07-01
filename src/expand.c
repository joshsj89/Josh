/*
 * expand.c
 *
 * This file contains the implementation of variable expansion logic.
 * 
 * TODO:
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

/*
 * Function: append_char
 * ----------------------
 * Appends a character to the destination buffer.
 *
 * Parameters:
 *   dest - A pointer to the destination buffer.
 *   len - A pointer to the current length of the destination buffer.
 *   capacity - A pointer to the current capacity of the destination buffer.
 *   c - The character to append.
 *
 * Returns:
 *   0 on success, -1 on failure.
 */
static int append_char(char **dest, size_t *len, size_t *capacity, char c)
{
    if (*len + 2 > *capacity) // +2 for the new character and null terminator
    {
        size_t new_capacity = *capacity * 2; // Double the capacity

        char *new_dest = realloc(*dest, new_capacity);

        if (new_dest == NULL)
            return -1; // Memory allocation failed

        *capacity = new_capacity;
        *dest = new_dest;
    }

    (*dest)[(*len)++] = c; // Append the character
    (*dest)[*len] = '\0'; // Null-terminate the string

    return 0; // Success
}

/**
 * Function: append_string
 * -----------------------
 * Appends a string to the destination buffer.
 *
 * Parameters:
 *   dest - A pointer to the destination buffer.
 *   len - A pointer to the current length of the destination buffer.
 *   capacity - A pointer to the current capacity of the destination buffer.
 *   src - The source string to append.
 * 
 * Returns:
 *   0 on success, -1 on failure.
 */
static int append_string(char **dest, size_t *len, size_t *capacity, const char *src)
{
    size_t src_len = strlen(src);
    
    // Need room for src plus null terminator
    if (*len + src_len + 1 > *capacity)
    {
        size_t new_capacity = *capacity;

        while (*len + src_len + 1 > new_capacity)
            new_capacity *= 2; // Double the capacity until it's enough

        char *new_dest = realloc(*dest, new_capacity);

        if (new_dest == NULL)
            return -1; // Memory allocation failed

        *capacity = new_capacity;
        *dest = new_dest;
    }

    memcpy(*dest + *len, src, src_len);
    *len += src_len;
    (*dest)[*len] = '\0'; // Null-terminate the string

    return 0; // Success
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
 * 
 * Note:
 *  The function returns a newly allocated string containing the value of the variable
 *  in order for the caller to be responsible for freeing the returned string like the 
 *  other expansion functions.
 */
static char *expand_variable(const char **input)
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

    char *expanded_value = strdup(var_value);

    if (expanded_value == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL; // Return NULL to indicate an error
    }
    return expanded_value;
}

static char *expand_command(const char **input)
{
    char command[1024];
    size_t i = 0;

    while (**input && **input != ')' && i < sizeof(command) - 1) // Read until the matching closing parenthesis or end of string
        command[i++] = *(*input)++;
    command[i] = '\0';

    if (**input == ')') // Move past the closing parenthesis if present
        (*input)++;
    else
    {
        fprintf(stderr, "Error: Missing closing ')' for command substitution\n");
        return NULL; // Return NULL to indicate an error
    }

    FILE *fp = popen(command, "r"); // Execute the command and open a pipe to read its output
    if (fp == NULL)
        return strdup(""); // If the command fails to execute, return an empty string

    char *output = malloc(1024);
    if (output == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        pclose(fp);
        return NULL; // Return NULL to indicate an error
    }

    size_t output_len = fread(output, 1, 1023, fp); // Read the command output
    output[output_len] = '\0'; // Null-terminate the output string
    pclose(fp); // Close the pipe

    while (output_len > 0 && (output[output_len - 1] == '\n' || output[output_len - 1] == '\r')) // Remove trailing newlines
        output[--output_len] = '\0';

    return output; // Return the command output
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
    size_t capacity = strlen(input) + 64; // Initial capacity for the output buffer (safe number to reduce reallocations)
    char *result = malloc(capacity);
    size_t len = 0;
    result[0] = '\0'; // Initialize the result string
    while (*input != '\0')
    {
        if (*input != '$')
        {
            if (append_char(&result, &len, &capacity, *input) == -1) // Append the character to the result
            {
                free(result);
                return NULL; // Return NULL to indicate an error
            }
            input++; // Move to the next character
        }
        else
        {
            input++; // Move past the '$' character
            if (*input == '\0') // If the string ends with '$', return an empty string
            {
                free(result);
                return strdup(""); // Return an empty string
            }

            if (isalpha(*input) || *input == '_') // Check if the next character is valid for a variable name
            {
                char *expanded_value = expand_variable(&input); // Expand the variable
                if (append_string(&result, &len, &capacity, expanded_value) == -1) // Append the variable value to the result
                {
                    free(expanded_value);
                    free(result);
                    return NULL;
                }
                free(expanded_value); // Free the allocated memory for the expanded value
            }
            else if (*input == '{') // Handle special case for '${VAR}' syntax
            {
                input++; // Move past the '{' character
                char *expanded_value = expand_variable(&input); // Expand the variable
                if (append_string(&result, &len, &capacity, expanded_value) == -1) // Append the variable value to the result
                {
                    free(expanded_value);
                    free(result);
                    return NULL;
                }
                free(expanded_value); // Free the allocated memory for the expanded value

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
            else if (*input == '(')
            {
                // input++; // Move past the '(' character

                if (*(input + 1) == ')') // Check for empty command substitution
                {
                    free(result);
                    return NULL; // Return NULL to indicate an error
                }

                if (*(input + 1) == '(') // Check for arithmetic expansion
                {
                    // Handle arithmetic expansion here (not implemented)
                    fprintf(stderr, "Error: Arithmetic expansion not implemented");
                    free(result);
                    return NULL; // Return NULL to indicate an error
                }
                else // Handle command substitution
                {
                    input++; // Move past the '(' character
                    char *command_result = expand_command(&input); // Expand the command
                    if (command_result == NULL)
                    {
                        free(result);
                        return NULL; // Return NULL to indicate an error
                    }
                    if (append_string(&result, &len, &capacity, command_result) == -1) // Append the command result to the output buffer
                    {
                        free(command_result);
                        free(result);
                        return NULL;
                    }
                    free(command_result); // Free the allocated memory for the command result
                }
            }
            else if (*input == '$') // Handle special case for '$$' (PID of the shell)
            {
                snprintf(result, 20, "%d", getpid()); // Convert PID to string
                len += strlen(result); // Update the length
                input++; // Move past the second '$'
            }
            else if (*input == '?') // Handle special case for '$?' (exit status of the last command)
            {
                // snprintf(result, 20, "%d", WEXITSTATUS(getpid())); // Convert exit status to string
                // len += strlen(result); // Update the length
                // input++; // Move past the '?'

                fprintf(stderr, "Error: Special variable '$?' not implemented");
                free(result);
                return NULL; // Return NULL to indicate an error
            }
            else
            {
                fprintf(stderr, "Error: Invalid variable name after '$'");
                free(result);
                return NULL; // Return NULL to indicate an error
            }
        }
    }

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
