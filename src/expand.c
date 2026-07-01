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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "expand.h"

typedef struct
{
    char *data;
    size_t len;
    size_t capacity;
} StringBuilder;

/*
 * Function: sb_init
 * -----------------
 * Initializes a StringBuilder with a specified initial capacity.
 *
 * Parameters:
 *   sb - A pointer to the StringBuilder to initialize.
 *   initial_capacity - The initial capacity of the StringBuilder.
 *
 * Returns:
 *   0 on success, -1 on failure (e.g., memory allocation failure).
 */
static int sb_init(StringBuilder *sb, size_t initial_capacity)
{
    sb->data = malloc(initial_capacity);

    if (sb->data == NULL)
        return -1; // Memory allocation failed

    sb->len = 0;
    sb->capacity = initial_capacity;
    sb->data[0] = '\0'; // Null-terminate the string

    return 0; // Success
}

/*
 * Function: sb_destroy
 * --------------------
 * Frees the memory allocated for the StringBuilder and resets its fields.
 *
 * Parameters:
 *   sb - A pointer to the StringBuilder to destroy.
 */
static void sb_destroy(StringBuilder *sb)
{
    free(sb->data);

    sb->data = NULL;
    sb->len = 0;
    sb->capacity = 0;
}

/*
 * Function: append_char
 * ----------------------
 * Appends a character to the destination buffer.
 *
 * Parameters:
 *   sb - A pointer to the StringBuilder to append to.
 *   c - The character to append.
 *
 * Returns:
 *   0 on success, -1 on failure.
 */
static int append_char(StringBuilder *sb, char c)
{
    if (sb->len + 2 > sb->capacity) // +2 for the new character and null terminator
    {
        size_t new_capacity = sb->capacity * 2; // Double the capacity

        char *new_dest = realloc(sb->data, new_capacity);

        if (new_dest == NULL)
            return -1; // Memory allocation failed

        sb->capacity = new_capacity;
        sb->data = new_dest;
    }

    sb->data[sb->len++] = c; // Append the character
    sb->data[sb->len] = '\0'; // Null-terminate the string

    return 0; // Success
}

/**
 * Function: append_string
 * -----------------------
 * Appends a string to the destination buffer.
 *
 * Parameters:
 *   sb - A pointer to the StringBuilder to append to.
 *   src - The source string to append.
 * 
 * Returns:
 *   0 on success, -1 on failure.
 */
static int append_string(StringBuilder *sb, const char *src)
{
    size_t src_len = strlen(src);
    
    // Need room for src plus null terminator
    if (sb->len + src_len + 1 > sb->capacity)
    {
        size_t new_capacity = sb->capacity;

        while (sb->len + src_len + 1 > new_capacity)
            new_capacity *= 2; // Double the capacity until it's enough

        char *new_dest = realloc(sb->data, new_capacity);

        if (new_dest == NULL)
            return -1; // Memory allocation failed

        sb->capacity = new_capacity;
        sb->data = new_dest;
    }

    memcpy(sb->data + sb->len, src, src_len);
    sb->len += src_len;
    sb->data[sb->len] = '\0'; // Null-terminate the string

    return 0; // Success
}

/*
 * Function: append_format
 * -----------------------
 * Appends a formatted string to the destination buffer.
 *
 * Parameters:
 *   sb - A pointer to the StringBuilder to append to.
 *   format - The format string (like printf).
 *   ... - Additional arguments for the format string.
 *
 * Returns:
 *   0 on success, -1 on failure.
 */
static int append_format(StringBuilder *sb, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    // Calculate the required size for the formatted string
    int required_size = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (required_size < 0)
        return -1; // Encoding error

    // Ensure there is enough capacity in the StringBuilder
    if (sb->len + required_size + 1 > sb->capacity)
    {
        size_t new_capacity = sb->capacity;

        while (sb->len + required_size + 1 > new_capacity)
            new_capacity *= 2; // Double the capacity until it's enough

        char *new_dest = realloc(sb->data, new_capacity);

        if (new_dest == NULL)
            return -1; // Memory allocation failed

        sb->capacity = new_capacity;
        sb->data = new_dest;
    }

    // Now actually write the formatted string into the buffer
    va_start(args, format);
    vsnprintf(sb->data + sb->len, sb->capacity - sb->len, format, args);
    va_end(args);

    sb->len += required_size; // Update the length of the StringBuilder

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

/*
 * Function: expand_command
 * ------------------------
 * Expands a command substitution and returns its output.
 *
 * Parameters:
 *   input - A pointer to the input string containing the command to expand.
 *
 * Returns:
 *   A pointer to the output of the command.
 * 
 * Note:
 *  The function returns a newly allocated string containing the output of the command
 *  in order for the caller to be responsible for freeing the returned string like the 
 *  other expansion functions.
 */
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
    StringBuilder sb;
    if (sb_init(&sb, capacity) == -1)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL; // Return NULL to indicate an error
    }
    
    while (*input != '\0')
    {
        if (*input != '$')
        {
            if (append_char(&sb, *input) == -1) // Append the character to the result
            {
                sb_destroy(&sb);
                return NULL; // Return NULL to indicate an error
            }
            input++; // Move to the next character
        }
        else
        {
            input++; // Move past the '$' character
            if (*input == '\0') // If the string ends with '$', return an empty string
            {
                sb_destroy(&sb);
                return strdup(""); // Return an empty string
            }

            if (isalpha(*input) || *input == '_') // Check if the next character is valid for a variable name
            {
                char *expanded_value = expand_variable(&input); // Expand the variable
                if (append_string(&sb, expanded_value) == -1) // Append the variable value to the result
                {
                    free(expanded_value);
                    sb_destroy(&sb);
                    return NULL;
                }
                free(expanded_value); // Free the allocated memory for the expanded value
            }
            else if (*input == '{') // Handle special case for '${VAR}' syntax
            {
                input++; // Move past the '{' character
                char *expanded_value = expand_variable(&input); // Expand the variable
                if (append_string(&sb, expanded_value) == -1) // Append the variable value to the result
                {
                    free(expanded_value);
                    sb_destroy(&sb);
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
                    sb_destroy(&sb);
                    return NULL; // Return NULL to indicate an error
                }
            }
            else if (*input == '(')
            {
                // input++; // Move past the '(' character

                if (*(input + 1) == ')') // Check for empty command substitution
                {
                    sb_destroy(&sb);
                    return NULL; // Return NULL to indicate an error
                }

                if (*(input + 1) == '(') // Check for arithmetic expansion
                {
                    // Handle arithmetic expansion here (not implemented)
                    fprintf(stderr, "Error: Arithmetic expansion not implemented");
                    sb_destroy(&sb);
                    return NULL; // Return NULL to indicate an error
                }
                else // Handle command substitution
                {
                    input++; // Move past the '(' character
                    char *command_result = expand_command(&input); // Expand the command
                    if (command_result == NULL)
                    {
                        sb_destroy(&sb);
                        return NULL; // Return NULL to indicate an error
                    }
                    if (append_string(&sb, command_result) == -1) // Append the command result to the output buffer
                    {
                        free(command_result);
                        sb_destroy(&sb);
                        return NULL;
                    }
                    free(command_result); // Free the allocated memory for the command result
                }
            }
            else if (*input == '$') // Handle special case for '$$' (PID of the shell)
            {
                append_format(&sb, "%d", getpid()); // Convert PID to string
                input++; // Move past the second '$'
            }
            else if (*input == '?') // Handle special case for '$?' (exit status of the last command)
            {
                // snprintf(result, 20, "%d", WEXITSTATUS(getpid())); // Convert exit status to string
                // len += strlen(result); // Update the length
                // input++; // Move past the '?'

                fprintf(stderr, "Error: Special variable '$?' not implemented");
                sb_destroy(&sb);
                return NULL; // Return NULL to indicate an error
            }
            else
            {
                fprintf(stderr, "Error: Invalid variable name after '$'");
                sb_destroy(&sb);
                return NULL; // Return NULL to indicate an error
            }
        }
    }

    return sb.data; // Return the expanded string (caller is responsible for freeing it)
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
