#include "syscall.h"

char *argv[5] = {"../test/execv", "arg1", "arg2", "arg3", "arg4"};
int
main(void)
{
    ExecV(5, argv);
    Halt();
}
