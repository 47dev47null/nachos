#include "syscall.h"

int
main(void)
{
    Exec("");
    ExecV("", 0);
    Halt();
}
