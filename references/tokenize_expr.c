#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/**
 * Parses a Bash expression, tracks parenthesis/bracket depth,
 * and verifies syntax correctness.
 *
 * @param expr The input Bash expression string.
 * @return true if parentheses are balanced, false otherwise.
 * @note 
 */
bool parse_bash_expression(const char *expr)
{
    int depth = 0;
    int max_depth = 0;

    for (int i = 0; expr[i] != '\0'; i++)
    {
        char current = expr[i];

        // Handle escape characters so we don't accidentally
        // parse escaped parentheses e.g., \( or \)
        if (current == '\\')
        {
            if (expr[i + 1] != '\0')
            {
                i++; // Skip the escaped character
            }
            continue;
        }

        // Parenthesis or Bracket Depth Tracking
        if (current == '(' || current == '[' || current == '{')
        {
            depth++;
            if (depth > max_depth)
                max_depth = depth;
        }
        else if (current == ')' || current == ']' || current == '}')
        {
            depth--;
            // If depth goes below 0, we have a closing parenthesis
            // before an opening one (syntax error)
            if (depth < 0)
            {
                printf("Syntax Error: Unbalanced closing parenthesis at index %d\n", i);
                return false;
            }
        }
    }

    // At the end of the string, depth should be exactly 0
    if (depth != 0)
    {
        printf("Syntax Error: Missing closing parenthesis. Max depth was %d\n", max_depth);
        return false;
    }

    printf("Syntax Valid! Max parenthesis depth: %d\n", max_depth);
    return true;
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

        parse_bash_expression(line);
    }

    return 0;
}
