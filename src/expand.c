/*
 * expand.c
 *
 * This file contains the implementation of variable expansion logic.
 * 
 * TODO:
 *  - Implement support for shell variables and special variables (e.g., $?, $!, etc.).
 *  - Build a custom parser for arithmetic expansion (e.g., $((expression))) rather than delegating to Bash.
 *  - Implement support for Bash string manipulation (e.g., ${VAR#pattern}, ${VAR%pattern}, etc.).
 *  - Implement support for escaping the '$' character (e.g., \$VAR should not expand).
 *  - Implement support for expanding variables in double-quoted strings while preserving spaces.
 */

#include <ctype.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "expand.h"
#include "parser.h"

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
 *   sb - A pointer to the StringBuilder to append the variable value to.
 *   input - A pointer to the input string containing the variable to expand.
 *
 * Returns:
 *   0 on success, -1 on failure.
 * 
 * Note:
 *  sb_destroy() is called in case of an error to free the StringBuilder and prevent memory leaks.
 */
static int expand_variable(StringBuilder *sb, const char **input)
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

    if (append_string(sb, var_value) == -1) // Append the variable value to the StringBuilder
    {
        sb_destroy(sb);
        return -1; // Return -1 to indicate an error
    }

    return 0; // Return 0 to indicate success
}

/*
 * Function: expand_command
 * ------------------------
 * Expands a command substitution and returns its output.
 *
 * Parameters:
 *   sb - A pointer to the StringBuilder to append the command output to.
 *   input - A pointer to the input string containing the command to expand.
 *
 * Returns:
 *   0 on success, -1 on failure.
 * 
 * Note:
 *  sb_destroy() is called in case of an error to free the StringBuilder and prevent memory leaks.
 */
static int expand_command(StringBuilder *sb, const char **input)
{
    char command[1024];
    size_t i = 0;

    if (**input && i < sizeof(command) - 1)
    {
        int cmd_length = command_substitution(*input); // Get the length of the command substitution inside $()

        if (cmd_length == -1) // Check for syntax error in command substitution
        {
            sb_destroy(sb);
            return -1; // Return -1 to indicate an error
        }

        memcpy(command + i, *input, cmd_length); // Copy the command substitution into the command buffer
        i += cmd_length;
        (*input) += cmd_length;
    }
    command[i] = '\0';

    if (**input == ')') // Move past the closing parenthesis if present
        (*input)++;
    else
    {
        fprintf(stderr, "Error: Missing closing ')' for command substitution\n");
        return -1; // Return -1 to indicate an error
    }

    FILE *fp = popen(command, "r"); // Execute the command and open a pipe to read its output
    if (fp == NULL) // Command execution failed
    {
        sb_destroy(sb);
        return -1;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) // Read the command output line by line
    {
        if (append_string(sb, buffer) == -1) // Append the command output line to the StringBuilder
        {
            pclose(fp);
            sb_destroy(sb);
            return -1; // Return -1 to indicate an error
        }
    }

    // Remove the trailing newline character from the full output
    while (sb->len > 0 && sb->data[sb->len - 1] == '\n')
        sb->data[--sb->len] = '\0';

    pclose(fp); // Close the pipe
    return 0; // Return 0 to indicate success
}

/*
 * Function: expand_arithmetic
 * ----------------------------
 * Expands an arithmetic expression and returns its value.
 *
 * Parameters:
 *   sb - A pointer to the StringBuilder to append the result to.
 *   input - A pointer to the input string containing the arithmetic expression.
 *
 * Returns:
 *   0 on success, -1 on failure.
 */
static int expand_arithmetic(StringBuilder *sb, const char **input)
{
    char expr[1024];
    char command[1024 + 10];
    int expr_length = expression(*input); // Get the length of the expression inside $(())
    if (expr_length == -1) // Check for syntax error in expression
    {
        sb_destroy(sb);
        return -1; // Return -1 to indicate an error
    }

    if (**input && expr_length < (int)sizeof(expr) - 1)
    {
        memcpy(expr, *input, expr_length); // Copy the expression into the expr buffer
        expr[expr_length] = '\0'; // Null-terminate the expr string
        (*input) += expr_length; // Move the input pointer past the expression
    }
    else
    {
        fprintf(stderr, "Error: Expression too long for arithmetic expansion\n");
        sb_destroy(sb);
        return -1; // Return -1 to indicate an error
    }

    if (**input == ')' && *(*input + 1) == ')') // Move past the closing parentheses if present
        (*input) += 2;
    else
    {
        fprintf(stderr, "Error: Missing closing '))' for arithmetic expansion\n");
        sb_destroy(sb);
        return -1; // Return -1 to indicate an error
    }
    
    snprintf(command, sizeof(command), "echo $((%s))", expr);

    FILE *fp = popen(command, "r"); // Execute the command and open a pipe to read its output
    if (fp == NULL) // Command execution failed
    {
        sb_destroy(sb);
        return -1;
    }

    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), fp) != NULL) // Read the command output
    {
        // Remove the trailing newline character from the output
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n')
            buffer[len - 1] = '\0';

        if (append_string(sb, buffer) == -1) // Append the command output to the StringBuilder
        {
            pclose(fp);
            sb_destroy(sb);
            return -1; // Return -1 to indicate an error
        }
    }

    pclose(fp); // Close the pipe
    return 0;
}

/*
 * Function: expand_tilde_token
 * ----------------------------
 * Expands a tilde token and updates its text.
 *
 * Parameters:
 *   token - A pointer to the Token to expand.
 */
static void expand_tilde_token(Token *token)
{
    char *text = token->segments[0].text; // tilde expansion is only relevant for the first segment of the token
    char *slash = strchr(text, '/'); // Find the first '/' in the token text
    char *tilde_prefix = NULL;

    if (slash) // if a slash is found, we have a tilde prefix followed by a path
        tilde_prefix = strndup(text, slash - text);
    else // if no slash is found, the entire token is a tilde prefix
        tilde_prefix = strdup(text);

    char *expanded = NULL;

    if (strcmp(tilde_prefix, "~") == 0) // Handle the case of ~
    {
        expanded = getenv("HOME"); // Get the value of $HOME

        if (expanded == NULL) // If $HOME is not set, use the current user's home directory
        {
            struct passwd *pw = getpwuid(getuid());
            if (pw)
                expanded = pw->pw_dir;
            else
                fprintf(stderr, "Error: Unable to determine home directory\n");
        }
    }
    else if (strncmp(tilde_prefix, "~+", 2) == 0) // Handle the case of ~+
    {
        expanded = getenv("PWD"); // Get the value of $PWD

        if (expanded == NULL)
            fprintf(stderr, "Error: PWD environment variable not set\n");
    }
    else if (strncmp(tilde_prefix, "~-", 2) == 0) // Handle the case of ~-
    {
        expanded = getenv("OLDPWD"); // Get the value of $OLDPWD
        
        // If $OLDPWD is not set, use the literal string
        if (expanded == NULL)
            expanded = "~-";
    }
    else if (tilde_prefix[0] == '~') // Handle the case of ~username
    {
        char *username = tilde_prefix + 1; // Skip the '~' character
        struct passwd *pw = getpwnam(username); // Get the passwd struct for the username
        if (pw)
            expanded = pw->pw_dir; // Get the home directory for the user
        else
            expanded = tilde_prefix; // If the user does not exist, use the literal string
    }

    if (expanded)
    {
        size_t expanded_len = strlen(expanded);
        size_t suffix_len = slash ? strlen(slash) : 0;
        char *new_text = malloc(expanded_len + suffix_len + 1); // +1 for null terminator

        if (new_text == NULL)
        {
            fprintf(stderr, "Error: Memory allocation failed during tilde expansion\n");
            free(tilde_prefix);
            return;
        }

        strcpy(new_text, expanded); // Copy the expanded prefix
        if (slash) // Append the suffix if it exists
            strcat(new_text, slash);

        free(token->segments[0].text); // Free the old token text
        token->segments[0].text = new_text; // Update the token text with the new expanded string
    }

    free(tilde_prefix);
}

/*
 * Function: expand_parameter_segment
 * -------------------------------
 * Expands parameter references in a token.
 *
 * Parameters:
 *   seg - A pointer to the segment to expand.
 */
static void expand_parameter_segment(Segment *seg)
{
    size_t capacity = strlen(seg->text) + 64; // Initial capacity for the output buffer (safe number to reduce reallocations)
    const char *input = seg->text;
    StringBuilder sb;
    if (sb_init(&sb, capacity) == -1)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return; // Return to indicate an error
    }
    
    while (*input)
    {
        if (*input != '$')
        {
            if (append_char(&sb, *input) == -1) // Append the character to the result
            {
                sb_destroy(&sb);
                return; // Return to indicate an error
            }
            input++; // Move to the next character
        }
        else
        {
            input++; // Move past the '$' character
            if (*input == '\0') // If the string ends with '$', return an empty string
            {
                sb_destroy(&sb);
                return; // Return to indicate an error
            }

            if (isalpha(*input) || *input == '_') // Check if the next character is valid for a variable name
            {
                if (expand_variable(&sb, &input) == -1) // Expand the variable
                    return;
            }
            else if (*input == '{') // Handle special case for '${VAR}' syntax
            {
                input++; // Move past the '{' character
                if (expand_variable(&sb, &input) == -1) // Append the variable value to the result
                    return;

                if (*input == '}') // Check for closing '}'
                {
                    input++; // Move past the '}' character
                }
                else
                {
                    fprintf(stderr, "Error: Missing closing '}' for variable expansion");
                    sb_destroy(&sb);
                    return; // Return to indicate an error
                }
            }
            else if (*input == '$') // Handle special case for '$$' (PID of the shell)
            {
                if (append_format(&sb, "%d", getpid()) == -1) // Convert PID to string
                {
                    sb_destroy(&sb);
                    return;
                }
                input++; // Move past the second '$'
            }
            else if (*input == '?') // Handle special case for '$?' (exit status of the last command)
            {
                // snprintf(result, 20, "%d", WEXITSTATUS(getpid())); // Convert exit status to string
                // len += strlen(result); // Update the length
                // input++; // Move past the '?'

                fprintf(stderr, "Error: Special variable '$?' not implemented");
                sb_destroy(&sb);
                return; // Return to indicate an error
            }
        }
    }

    free(seg->text); // Free the old text to prevent memory leaks
    seg->text = sb.data; // Update the segment text with the expanded string
    return; // Return to indicate successful expansion
}

/*
 * Function: expand_command_arithmetic_segment
 * ----------------------------------------
 * Expands command and arithmetic expansions in a segment.
 *
 * Parameters:
 *   seg - A pointer to the segment to expand.
 */
static void expand_command_arithmetic_segment(Segment *seg)
{
    size_t capacity = strlen(seg->text) + 64; // Initial capacity for the output buffer (safe number to reduce reallocations)
    const char *input = seg->text;
    StringBuilder sb;
    if (sb_init(&sb, capacity) == -1)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return; // Return to indicate an error
    }
    
    while (*input)
    {
        if (*input != '$')
        {
            if (append_char(&sb, *input) == -1) // Append the character to the result
            {
                sb_destroy(&sb);
                return; // Return to indicate an error
            }
            input++; // Move to the next character
        }
        else
        {
            input++; // Move past the '$' character
            if (*input == '\0') // If the string ends with '$', return an empty string
            {
                sb_destroy(&sb);
                return; // Return to indicate an error
            }

            if (*input == '(')
            {
                // input++; // Move past the '(' character

                if (*(input + 1) == ')') // Check for empty command substitution
                {
                    sb_destroy(&sb);
                    return; // Return to indicate an error
                }

                if (*(input + 1) == '(') // Check for arithmetic expansion
                {
                    input += 2; // Move past the '((' characters
                    if (expand_arithmetic(&sb, &input) == -1)
                        return; // Return to indicate an error
                }
                else // Handle command substitution
                {
                    input++; // Move past the '(' character
                    if (expand_command(&sb, &input) == -1)
                        return; // Return to indicate an error
                }
            }
        }
    }

    free(seg->text); // Free the old text to prevent memory leaks
    seg->text = sb.data; // Update the segment text with the expanded string
    return; // Return to indicate successful expansion
}

/*
 * Function: rebuild_token
 * ------------------------
 * Rebuilds the full_text of a token by concatenating all its segments.
 *
 * Parameters:
 *   token - A pointer to the Token structure to rebuild.
 * 
 * Returns:
 *   A pointer to the newly allocated string containing the concatenated segments.
 *  
 * Note:
 *   The caller is responsible for freeing this string when it is no longer needed.
 */
static char *rebuild_token(const Token *token)
{
    StringBuilder sb;
    if (sb_init(&sb, 64) == -1)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL; // Return NULL to indicate an error
    }

    for (size_t i = 0; i < token->segment_count; i++)
    {
        Segment *seg = &token->segments[i];
        if (append_string(&sb, seg->text) == -1)
        {
            sb_destroy(&sb);
            return NULL; // Return to indicate an error
        }
    }

    return sb.data; // Return the new concatenated string
}

/*
 * Function: is_ifs
 * ----------------
 * Checks if a character is part of the IFS (Internal Field Separator).
 *
 * Parameters:
 *   c - The character to check.
 *
 * Returns:
 *   true if the character is part of IFS, false otherwise.
 */
static bool is_ifs(char c)
{
    const char *ifs = getenv("IFS");

    if (c == '\0') // since strchr includes the null terminator
        return false;
    
    // Default if IFS is not set
    if (ifs == NULL)
        ifs = " \t\n";

    return strchr(ifs, c) != NULL;
}

/*
 * Function: scan_field
 * --------------------
 * Extracts a field from the input string based on IFS.
 *
 * Parameters:
 *   input - A pointer to the input string.
 *   token - A pointer to the token to store the extracted field.
 *
 * Returns:
 *   true if a field was extracted, false if no field is found.
 */
static bool scan_field(char **input, Token *token)
{
    token_init(token); // Initialize the token structure
    
    // Skip leading IFS characters
    while (is_ifs(**input))
        (*input)++;

    // If we reach the end of the string
    if (**input == '\0')
        return false; 

    char *start = *input; // Mark the start of the field

    // Move to the end of the field
    while (**input != '\0' && !is_ifs(**input))
        (*input)++;

    token->full_text = strndup(start, *input - start); // Set the new token text to the extracted field

    return true;
}

/*
 * Function: append_token
 * ----------------------
 * Appends a token to an array of tokens.
 *
 * Parameters:
 *   argv - A pointer to the array of tokens.
 *   argc - A pointer to the number of tokens in the array.
 *   capacity - A pointer to the capacity of the array.
 *   new_token - The token to append.
 */
static void append_token(Token **argv, size_t *argc, size_t *capacity, Token *new_token)
{
    // If we exceed the buffer size, reallocate more space
    if (*argc >= *capacity)
    {
        *capacity += INITIAL_TOKEN_CAPACITY;
        *argv = realloc(*argv, (*capacity) * sizeof(Token)); // Allocate memory for the new array of tokens

        if (*argv == NULL)
        {
            fprintf(stderr, "Allocation error\n");
            return;
        }
    }
    
    (*argv)[*argc] = *new_token; // Copy the new token into the array
    (*argc)++; // Increment the argument count
}

/*
 * Function: word_split
 * --------------------
 * Splits a token into words based on IFS and appends them to an array of tokens.
 *
 * Parameters:
 *   token - A pointer to the token to split.
 *   argv - A pointer to the array of tokens.
 *   argc - A pointer to the number of tokens in the array.
 *   capacity - A pointer to the capacity of the array.
 */
static void word_split(Token *token, Token **argv, size_t *argc, size_t *capacity)
{
    // split the token text into words based on IFS
    char *p = token->full_text;
    Token field;

    while ((scan_field(&p, &field))) 
        append_token(argv, argc, capacity, &field); // Append the newly created field token to the array of tokens

    free_token(token); // Free the original token to prevent memory leaks
}

/*
 * Function: expand_pathname
 * -------------------------
 * Expands pathname expressions in a command.
 *
 * Parameters:
 *   cmd - A pointer to the command whose tokens to expand.
 */
static void expand_pathname(Command *cmd)
{
    (void)cmd; // Suppress unused parameter warning
    
    /*
    for (size_t i = 0; i < cmd->argc; i++)
    {
        Token *token = &cmd->argv[i];
        if (token->single_quoted || token->double_quoted)
        continue; // No expansion needed for quoted tokens

    if (!token->pathname_expand)
        continue; // No expansion needed if there are no pathname expansions
    
    fprintf(stderr, "Error: Pathname expansion not implemented");
    */
}

/**
 * Function: needs_word_split
 * --------------------------
 * Determines if a token requires word splitting.
 *
 * Parameters:
 *   token - A pointer to the token to check
 *
 * Returns:
 *   true if the token requires word splitting, false otherwise
 */
static bool needs_word_split(const Token *token)
{
    for (size_t j = 0; j < token->segment_count; j++)
    {
        Segment *seg = &token->segments[j];
        if (seg->allow_word_split) // Check if the segment allows word splitting
            return true; // If any segment allows word splitting, return true
    }
    return false; // No segments allow word splitting
}

/*
 * Function: expand_variables
 * --------------------------
 * Expands variables, command substitutions, and arithmetic expressions in all tokens of a command.
 *
 * Parameters:
 *   cmd - A pointer to the Command structure containing the tokens to expand.
 */
void expand_variables(Command *cmd)
{
    size_t bufsize = INITIAL_TOKEN_CAPACITY;
    Token *new_argv = malloc(bufsize * sizeof(Token));
    size_t new_argc = 0;
    
    for (size_t i = 0; i < cmd->argc; i++)
    {
        Token *token = &cmd->argv[i];

        expand_tilde_token(token);

        for (size_t j = 0; j < token->segment_count; j++)
        {
            Segment *seg = &token->segments[j];

            switch (seg->type)
            {
                case SEG_LITERAL:
                    // No expansion needed for literal segments
                    break;
                case SEG_VARIABLE:
                    if (seg->quote != QUOTE_SINGLE) // Only expand variables if not single-quoted
                        expand_parameter_segment(seg);
                    break;
                case SEG_COMMAND:
                case SEG_ARITHMETIC:
                    if (seg->quote != QUOTE_SINGLE) // Only expand command and arithmetic segments if not single-quoted
                        expand_command_arithmetic_segment(seg);
                    break;
                default:
                    fprintf(stderr, "Error: Unknown segment type\n");
                    break;
            }
        }

        free(token->full_text);
        token->full_text = rebuild_token(token); // Rebuild the token's text after expansion
        
        if (needs_word_split(token)) // Check if the token requires word splitting
            word_split(token, &new_argv, &new_argc, &bufsize); // Split the token into words and append to new_argv
        else
            append_token(&new_argv, &new_argc, &bufsize, token); // Append the token as is to new_argv
    }

    free(cmd->argv);
    cmd->argv = new_argv;
    cmd->argc = new_argc;
    
    expand_pathname(cmd);
}
