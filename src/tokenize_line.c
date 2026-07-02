#include <ctype.h>
#include <stdio.h>
#include <string.h>

/*
 * Function: tokenize_line
 * ------------------------
 * Tokenizes a line of input into individual tokens.
 *
 * This function takes a line of input and splits it into individual tokens.
 * It returns a dynamically allocated string containing the next token from the input.
 *
 * Parameters:
 *   line - A pointer to the input string to be tokenized.
 *
 * Returns:
 *   A pointer to a dynamically allocated string containing the next token, or NULL if no more tokens are available.
 */
static char *tokenize_line(char *line)
{
    int in_single_quotes = 0;
    int in_double_quotes = 0;
    int paren_depth = 0;

    static char *token_start = NULL; // Pointer to the start of the current token

    // Continue from the last position if line is NULL
    if (line == NULL)
        line = token_start;

     // If the line is empty, reset token_start and return NULL
    if (*line == '\0')
    {
        token_start = NULL;
        return NULL;
    }

    // Skip leading whitespace
    while (isspace(*line))
        line++;

    // If the line is empty after skipping whitespace
    if (*line == '\0') 
    {
        token_start = NULL; // Reset token_start
        return NULL;
    }

    char *token_end = line; // Pointer to the end of the current token
    while (*token_end != '\0')
    {
        // Stop tokenizing at whitespace if not in quotes or parentheses
        if (!in_single_quotes && !in_double_quotes && paren_depth == 0 && isspace(*token_end))
            break; 

        if (*token_end == '\'' && !in_double_quotes && paren_depth == 0)
            in_single_quotes = !in_single_quotes; // Toggle single quote state
        else if (*token_end == '\"' && !in_single_quotes && paren_depth == 0)
            in_double_quotes = !in_double_quotes; // Toggle double quote state
        else if (*token_end == '(' && !in_single_quotes && !in_double_quotes)
            paren_depth++; // Increase parenthesis depth
        else if (*token_end == ')' && !in_single_quotes && !in_double_quotes)
            paren_depth--; // Decrease parenthesis depth
        else if (*token_end == '$' && *(token_end + 1) == '(')
        {
            paren_depth++; // Increase parenthesis depth for $(
            token_end++; // Skip the next character (the opening parenthesis)
        }
        else if (*token_end == ')' && paren_depth > 0)
            paren_depth--; // Decrease parenthesis depth for )
        else if (*token_end == '\\')
            token_end++; // Skip the next character (escaped character will be treated as a literal)

        token_end++;
    }

    if (paren_depth != 0)
    {
        fprintf(stderr, "Error: Unmatched parentheses in input\n");
        token_start = NULL; // Reset token_start
        return NULL; // Return NULL to indicate an error
    }

    // If we reached the end of the line, return the last token
    if (*token_end == '\0')
    {
        token_start = token_end;
        return line;
    }

    *token_end = '\0'; // Terminate the token
    token_start = token_end + 1; // Update the start position for the next token
    return line;
}

int main() 
{
    char line[256];
    while (1) 
    {
        printf("josh> ");
        if (fgets(line, sizeof(line), stdin) == NULL) 
        {
            // fprintf(stderr, "Error reading input\n");
            printf("\n"); // Print a newline for better formatting
            return 0;
        }

        // Remove the newline character from the input
        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, "exit") == 0) 
        {
            break; // Exit the loop if the user types 'exit'
        }

        char *token = tokenize_line(line);
        while (token != NULL) 
        {
            printf("Token: %s\n", token);
            token = tokenize_line(NULL);
        }
    }

    // scanf("%m[^\n]", &line); // Read a line of input from the user
    if (fgets(line, sizeof(line), stdin) == NULL) 
    {
        fprintf(stderr, "Error reading input\n");
        return 1;
    }

    char *token = tokenize_line(line);
    while (token != NULL) 
    {
        printf("Token: %s\n", token);
        token = tokenize_line(NULL);
    }

    return 0;
}