#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    FILE *fp = popen("echo $((3 ++ 5))", "r");
    if (fp == NULL) return -1;

    char buffer[128];
    if (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        printf("Result: %s", buffer);
    }

    pclose(fp);

    return 0;
}
