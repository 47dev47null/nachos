#include "syscall.h"

int strcmp(char *a, char *b)
{
    while (a && b && *a != '\0' && *b != '\0')
    {
        if (*a != *b)
            return 0;
        a++; b++;
    }
    if (*a == '\0' && *b == '\0')
        return 1;
    else
        return 0;
}

int
main(int argc, char *argv[])
{
    if (argc == 5 &&
            argv[1][0] == 'a' &&
            argv[1][1] == 'r' &&
            argv[1][2] == 'g' &&
            argv[1][3] == '1' &&
            argv[2][0] == 'a' &&
            argv[2][1] == 'r' &&
            argv[2][2] == 'g' &&
            argv[2][3] == '2' &&
            argv[3][0] == 'a' &&
            argv[3][1] == 'r' &&
            argv[3][2] == 'g' &&
            argv[3][3] == '3' &&
            argv[4][0] == 'a' &&
            argv[4][1] == 'r' &&
            argv[4][2] == 'g' &&
            argv[4][3] == '4')
        Add(1, 2);      // stub syscall to ensure we meet the if condition
    Halt();
}
