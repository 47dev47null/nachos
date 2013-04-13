#include "syscall.h"

int
main()
{
    int rc = Create("hehe");
    Exit(rc);
}
