#include "syscall.h"

int
main()
{
    int pid = 0;
    pid = Fork();
    while (pid > 0)     // spin father process
        ;
    Exec("../test/exec");   // child process executes exec
}
