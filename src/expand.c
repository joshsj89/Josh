/*
 * expand.c
 *
 * This file contains the implementation of variable expansion logic.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "expand.h"

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

            // Extract the variable name
            char var_name[256];
            size_t i = 0;
            while (*input != '\0' && *input != ' ' && *input != '$' && (isalnum(*input) || *input == '_') && i < sizeof(var_name) - 1)
                var_name[i++] = *input++;
            var_name[i] = '\0';

            // Get the value of the environment variable
            const char *var_value = getenv(var_name);
            if (var_value == NULL)
                var_value = ""; // If the variable is not set, use an empty string

            // Append the variable value to the result
            size_t value_len = strlen(var_value);
            memcpy(ptr, var_value, value_len);
            ptr += value_len;
        }
    }

    *ptr = '\0'; // Null-terminate the result string
    return result;
}

void expand_variables(char **tokens)
{
    for (int i = 0; tokens[i] != NULL; i++)
        tokens[i] = expand_string(tokens[i]);
}
