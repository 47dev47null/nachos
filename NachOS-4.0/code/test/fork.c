#include "syscall.h"

int
main()
{
    int pid = 0;
    pid = Fork();
    while (pid > 0)
        ;
    Halt();
}
