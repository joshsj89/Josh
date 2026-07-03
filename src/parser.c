/*
 *      parser.c
 *
 *      This file contains the implementation of the command parser for the shell.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "tokens.h"

#define INITIAL_TOKEN_CAPACITY 64 // Initial capacity for the token array

void token_init(Token *token)
{
    token->type = TOKEN_WORD;
    token->text = NULL;
    token->quote = QUOTE_NONE;
    token->single_quoted = false;
    token->double_quoted = false;
    token->escaped = false;
    token->contains_variable = false;
    token->contains_command_substitution = false;
    token->contains_arithmetic = false;
    token->field_split = false;
    token->pathname_expand = false;
}

void token_add_text(Token *token, const char *text, size_t length)
{
    if (token->text == NULL)
    {
        token->text = malloc(length + 1);
        if (token->text == NULL)
        {
            fprintf(stderr, "Memory allocation failed\n");
            exit(EXIT_FAILURE);
        }
        memcpy(token->text, text, length);
        token->text[length] = '\0'; // Null-terminate the string
    }
}

void token_print(const Token *token)
{
    printf("Token Type: %d\n", token->type);
    printf("Text: %s\n", token->text ? token->text : "NULL");
    printf("Quote Type: %d\n", token->quote);
    printf("Single Quoted: %s\n", token->single_quoted ? "true" : "false");
    printf("Double Quoted: %s\n", token->double_quoted ? "true" : "false");
    printf("Escaped: %s\n", token->escaped ? "true" : "false");
    printf("Contains Variable: %s\n", token->contains_variable ? "true" : "false");
    printf("Contains Command Substitution: %s\n", token->contains_command_substitution ? "true" : "false");
    printf("Contains Arithmetic: %s\n", token->contains_arithmetic ? "true" : "false");
    printf("Field Split: %s\n", token->field_split ? "true" : "false");
    printf("Pathname Expand: %s\n", token->pathname_expand ? "true" : "false");
    printf("------------------------------\n");
}

/**
 * Parses a Bash expression, tracks parenthesis depth
 * and verifies syntax correctness.
 *
 * @param input The input Bash expression string.
 * @return The length of the expression if parentheses are balanced, -1 otherwise.
 * @note This function assumes the input string begins at the first character of the expression and does not skip leading whitespace.
 * The length returned is the number of characters processed in the expression. 
 * input + length will point to the character after the expression, which is useful for further processing of the input string.
 */
static int expression(const char *input)
{
    int depth = 0; // Track the depth of nested parentheses
    int length = 0; // Track the length of the expression

    while (*input)
    {
        if (*input == '(')
            depth++;
        else if (*input == ')')
        {
            depth--;

            // If depth goes below 0, we have a closing parenthesis
            // before an opening one (syntax error)
            if (depth < 0)
            {
                fprintf(stderr, "Syntax Error: Unbalanced closing parenthesis");
                return -1; // Return -1 to indicate an error
            }
        }

        length++; // Increment the length of the expression
        input++;
        
        if (depth == 0 && *input == ')') // If we are at the top level and encounter a closing parenthesis, stop
            break;
    }

    return length;
}

/**
 * Parses a simple command, handling quotes and escape characters.
 *
 * @param input The input string for the simple command.
 * @return The length of the simple command if parsed successfully, -1 otherwise.
 */
int simple_command(const char *input)
{
    int length = 0; // Track the length of the simple command
    bool in_single = false; // Track if we are inside single quotes
    bool in_double = false; // Track if we are inside double quotes

    while (*input)
    {
        if (*input == '\'' && !in_double)
            in_single = !in_single; // Toggle single quote state
        else if (*input == '\"' && !in_single)
            in_double = !in_double; // Toggle double quote state
        else if (*input == '\\') // Handle escape character
        {
            input++; // Skip the next character (escaped character will be treated as a literal)
            length++; // Count the escaped character
        }
        else if (!in_single && !in_double)
        {
            if (*input == '$' && *(input + 1) == '(' && *(input + 2) == '(')
            {
                input += 3; // Move past the '$' and the two '(' characters
                length += 3; // Count the '$' and the two '(' characters
                
                int expr_length = expression(input); // Get the length of the expression inside $((
                if (expr_length == -1) // Check for syntax error in expression
                    return -1; // Return -1 to indicate an error

                input += expr_length; // Skip to the end of the expression inside $(())
                length += expr_length; // Count the characters in the expression

                continue;
            }
            
            if (*input == '$' && *(input + 1) == '(')
            {
                input += 2; // Move past the '$' and the '(' characters
                length += 2; // Count the '$' and the '(' characters
                
                int cmd_length = simple_command(input); // Get the length of the simple command inside $()
                if (cmd_length == -1) // Check for syntax error in simple command
                    return -1; // Return -1 to indicate an error

                input += cmd_length; // Skip to the end of the simple command inside $()
                length += cmd_length; // Count the characters in the simple command

                continue;
            }
            
            if (*input == ')')
                break;
        }

        length++; // Increment the length of the simple command
        input++;
    }

    if (in_double)
    {
        fprintf(stderr, "Error: Unmatched double quotes in input");
        return -1; // Return -1 to indicate an error
    }

    if (in_single)
    {
        fprintf(stderr, "Error: Unmatched single quotes in input");
        return -1; // Return -1 to indicate an error
    }

    return length; // Return the length of the simple command
}

/*
 * Function: tokenize_line
 * ------------------------
 * Tokenizes a line of input into individual tokens.
 *
 * This function takes a line of input and splits it into individual tokens.
 * It handles quotes, escape characters, and special characters like variables and command substitutions.
 * It updates the provided Token structure with the parsed token information.
 *
 * Parameters:
 *   line - A pointer to the input string to be tokenized.
 *   token - A pointer to a Token structure to store the parsed token.
 * 
 * Note:
 *  This function maintains a static pointer to the start of the current token, allowing it to continue parsing 
 *  from where it left off in subsequent calls (similar to strtok()). For subsequent calls, pass NULL as the line
 *  parameter to continue tokenizing the same input string. token's text will be NULL when there are no more tokens
 *  to parse.
 */
static void tokenize_line(char *line, Token *token)
{
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    token_init(token); // Initialize the token structure

    static char *token_start = NULL; // Pointer to the start of the current token

    // Continue from the last position if line is NULL
    if (line == NULL)
        line = token_start;

     // If the line is empty, reset token_start and return NULL
    if (*line == '\0')
    {
        token_start = NULL;
        return;
    }

    // Skip leading whitespace
    while (isspace(*line))
        line++;

    // If the line is empty after skipping whitespace
    if (*line == '\0') 
    {
        token_start = NULL; // Reset token_start
        return;
    }

    char *token_end = line; // Pointer to the end of the current token
    while (*token_end)
    {
        // Stop tokenizing at whitespace if not in quotes or parentheses
        if (!in_single_quotes && !in_double_quotes && isspace(*token_end))
            break;

        if (*token_end == '\'' && !in_double_quotes)
        {
            in_single_quotes = !in_single_quotes; // Toggle single quote state
            token->single_quoted = true;
        }
        else if (*token_end == '\"' && !in_single_quotes)
        {
            in_double_quotes = !in_double_quotes; // Toggle double quote state
            token->double_quoted = true;
        }
        else if (*token_end == '$' && *(token_end + 1) == '(' && *(token_end + 2) == '(')
        {
            token->contains_arithmetic = true;
            
            token_end += 3; // Skip past the next three characters (the opening parentheses)

            int expr_length = expression(token_end); // Get the length of the expression inside $(())
            if (expr_length == -1) // Check for syntax error in expression
            {
                token_start = NULL; // Reset token_start
                return; // Return to indicate an error   
            }
            token_end += expr_length; // Skip past the end of the expression inside $(())

            if (*token_end != ')' || *(token_end + 1) != ')') // Ensure we have a closing parentheses
            {
                fprintf(stderr, "Error: Unmatched opening parenthesis in input");
                token_start = NULL; // Reset token_start
                return; // Return to indicate an error
            }

            token_end += 2; // Skip past the closing parentheses
            continue;
        }
        else if (*token_end == '$' && *(token_end + 1) == '(')
        {
            token->contains_command_substitution = true;
            
            token_end += 2; // Skip past the next two characters (the opening parenthesis)

            int cmd_length = simple_command(token_end); // Get the length of the simple command inside $()
            if (cmd_length == -1) // Check for syntax error in simple command
            {
                token_start = NULL; // Reset token_start
                return; // Return to indicate an error   
            }
            token_end += cmd_length; // Skip past the end of the simple command inside $()

            if (*token_end != ')') // Ensure we have a closing parenthesis
            {
                fprintf(stderr, "Error: Unmatched opening parenthesis in input");
                token_start = NULL; // Reset token_start
                return; // Return to indicate an error
            }

            token_end++; // Skip past the closing parenthesis
            continue;
        }
        else if (*token_end == '$' && (isalpha(*(token_end + 1)) || *(token_end + 1) == '_' || *(token_end + 1) == '$' || *(token_end + 1) == '?')) // Check for variable expansion
        {
            token->contains_variable = true;
            token_end++; // Move past the '$' character

            while (isalnum(*token_end) || *token_end == '_') // Move through the variable name
                token_end++;
            continue;
        }
        else if (*token_end == '$' && *(token_end + 1) == '{') // Check for variable expansion with braces
        {
            token->contains_variable = true;
            token_end += 2; // Move past the '${' characters

            while (isalnum(*token_end) || *token_end == '_') // Move through the variable name
                token_end++;

            if (*token_end != '}') // Ensure we have a closing brace
            {
                fprintf(stderr, "Error: Unmatched opening brace in input");
                token_start = NULL; // Reset token_start
                return; // Return to indicate an error
            }

            token_end++; // Move past the closing brace
            continue;
        }
        else if (*token_end == '\\')
        {
            token->escaped = true;
            token_end++; // Skip the next character (escaped character will be treated as a literal)
        }
        else if (!in_single_quotes && !in_double_quotes && *token_end == ')') // Stop tokenizing at closing parenthesis if not in quotes
        {
            fprintf(stderr, "syntax error near unexpected token `)'");
            token_start = NULL; // Reset token_start
            return; // Return to indicate an error
        }

        token_end++;
    }

    if (in_double_quotes)
    {
        fprintf(stderr, "Error: Unmatched double quotes in input");
        token_start = NULL; // Reset token_start
        return; // Return to indicate an error
    }

    if (in_single_quotes)
    {
        fprintf(stderr, "Error: Unmatched single quotes in input");
        token_start = NULL; // Reset token_start
        return; // Return to indicate an error
    }

    token_add_text(token, line, token_end - line); // Add the token text to the Token structure
    // token_print(token); // Print the token for debugging purposes

    // If we reached the end of the line, return the last token
    if (*token_end == '\0')
    {
        token_start = token_end;
        return; // Return to indicate no more tokens
    }

    *token_end = '\0'; // Terminate the token
    token_start = token_end + 1; // Update the start position for the next token
}

/**
 * Function: parse_line
 * ----------------------
 * Parses a line of input into an array of strings.
 *
 * This function takes a line of input and splits it into individual tokens based on whitespace.
 * It returns a dynamically allocated array of strings, where each string is a token from the input.
 *
 * Parameters:
 *   line - A pointer to the input string to be parsed.
 *
 * Returns:
 *   A pointer to a Command structure containing the parsed tokens and the number of tokens.
 * 
 * Note:
 *   The caller is responsible for freeing the memory allocated for the array of strings using free_command.
 * 
 */
Command *parse_line(char *line)
{
    size_t bufsize = INITIAL_TOKEN_CAPACITY;
    Command *command = malloc(sizeof(Command)); // Allocate memory for the Command structure
    if (!command)
    {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
    }

    command->argv = malloc(bufsize * sizeof(Token)); // Allocate memory for the array of token strings
    command->argc = 0; // Initialize the argument count to 0
    Token token;

    if (!command->argv)
    {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
    }

    tokenize_line(line, &token); // Get the first token from the input line
    while (token.text != NULL)
    {
        command->argv[command->argc++] = token;

        // If we exceed the buffer size, reallocate more space
        if (command->argc >= bufsize)
        {
            bufsize += INITIAL_TOKEN_CAPACITY;
            command->argv = realloc(command->argv, bufsize * sizeof(Token));
            if (!command->argv)
            {
                fprintf(stderr, "Allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        tokenize_line(NULL, &token); // Continue tokenizing the input line
    }

    return command;
}

/**
 * Function: free_command
 * ----------------------
 * Frees the memory allocated for a Command structure and its tokens. This function takes 
 * a pointer to a Command structure and frees the memory allocated for each token, as well
 * as the memory allocated for the array itself and the Command structure.
 *
 * Parameters:
 *   cmd - A pointer to the Command structure containing the tokens to be freed.
 * 
 */
void free_command(Command *cmd)
{
    if (!cmd || !cmd->argv) // Check for NULL pointers
        return; // Nothing to free

    for (size_t i = 0; i < cmd->argc; i++)
        free(cmd->argv[i].text); // Free the text of each token

    // Free the array of token pointers
    free(cmd->argv);
    free(cmd); // Free the Command structure itself
}
