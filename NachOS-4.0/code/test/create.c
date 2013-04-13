#include "syscall.h"

int
main()
{
    int rc = Create("hehe");
    rc = Remove("hehe");
    Exit(rc);
}
