#include "syscall.h"

char *argv[5] = {"~ProgName~", "~ARG1~", "~ARG2~", "~ARG3~", "~ARG4~"};
int
main(void)
{
    ExecV(5, argv);
    Halt();
}
