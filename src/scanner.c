#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * is_ifs - Check if a character is in the IFS (Internal Field Separator)
 * @param c The character to check
 * @return true if the character is in IFS, false otherwise
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
 * scan_field - Extract a field from the input string based on IFS
 * @param input Pointer to the input string
 * @return A newly allocated string containing the extracted field, or NULL if no field is found
 */
static char *scan_field(char **input)
{
    while (is_ifs(**input))
        (*input)++; // Skip leading IFS characters

    if (**input == '\0')
        return NULL; // Return NULL if we reach the end of the string

    char *start = *input; // Mark the start of the field

    while (**input != '\0' && !is_ifs(**input))
        (*input)++; // Move to the end of the field

    return strndup(start, *input - start); // Return a newly allocated string containing the field
}

int main(void)
{
    char token[256];
    while (1) 
    {
        printf("scanner> ");
        if (fgets(token, sizeof(token), stdin) == NULL) 
        {
            // fprintf(stderr, "Error reading input\n");
            printf("\n"); // Print a newline for better formatting
            return 0;
        }

        // Remove the newline character from the input
        token[strcspn(token, "\n")] = '\0';

        if (strcmp(token, "exit") == 0) 
        {
            break; // Exit the loop if the user types 'exit'
        }

        char *p = token;
        char *field;

        while ((field = scan_field(&p)) != NULL) 
        {
            printf("Field: %s\n", field);
            free(field); // Free the allocated memory for the field
        }
    }

    return 0;
}