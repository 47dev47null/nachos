#include "syscall.h"

int
main()
{
    int pid;
    pid = Fork();
    if (pid == 0)   // child process
        Exec("../test/exec");
    else
    {               // parent process
        int rc = Join(pid);
        if (rc == 10)
            Add(5, 5);
    }
    Halt();
}
