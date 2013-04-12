/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls 
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__ 
#define __USERPROG_KSYSCALL_H__ 

#include "kernel.h"

#define MAX_FILENAME_LEN 20

// ReadStr
// read string from virtaddr to buf
// stop when meet '\0' or size characters readed.
// return number of characters readed, including '\0'
static int ReadStr(int virtAddr, char *buf, int size)
{
    if (size <= 0)
        return -1;

    DEBUG(dbgSys, "read string from VA: " << virtAddr << " to kernel buffer.");

    char *sp = buf;
    do {
        kernel->machine->ReadMem(virtAddr++, sizeof(char), (int *)sp);
    } while (*sp++ != '\0' && sp < buf + size);
    DEBUG(dbgSys, "Read String: " << buf);
    return sp - buf;
}

void SysHalt()
{
  kernel->interrupt->Halt();
}


int SysAdd(int op1, int op2)
{
  return op1 + op2;
}

static void SysFork_Helper(void *nop)                                           
{                                                                               
    kernel->currentThread->RestoreUserState();                                  
    // Child process return 0.
    kernel->machine->WriteRegister(2, 0);
    kernel->machine->Run();                                                     
    ASSERTNOTREACHED();                                                         
}                                                                               
                                                                                
int SysFork()                                                                   
{                                                                               
    AddrSpace *dup = kernel->currentThread->space->Fork();                      
                                                                                
    if (dup == NULL)                                                            
        return ENOMEM;                                                          
                                                                                
    Thread *child = new Thread("child");                                        
    dup->proc->thread = child;                                                  
    child->space = dup;                                                         
    child->SaveUserState();                                                     

    child->WriteUserRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
    child->WriteUserRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
    child->WriteUserRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);

    child->Fork(SysFork_Helper, 0);                                             

    DEBUG(dbgSys, "[System Call] Forked process " << dup->proc->pid <<".");

    kernel->currentThread->Yield();                                             

    return dup->proc->pid;                                                      
}

int SysExec(int uname)
{
    char *kname = new char[MAX_FILENAME_LEN];
    if (ReadStr(uname, kname, MAX_FILENAME_LEN) == -1)
        return 1;
    if (kernel->currentThread->space->Load(kname) == FALSE)
        return 1;
    delete[] kname;
    DEBUG(dbgSys, "[System Call] New Executable Loaded.");
    kernel->currentThread->space->Execute();
    ASSERTNOTREACHED();
    return 0;
}

#endif /* ! __USERPROG_KSYSCALL_H__ */
