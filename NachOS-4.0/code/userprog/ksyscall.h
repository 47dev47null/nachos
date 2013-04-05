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
        return -ENOMEM;                                                          
                                                                                
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

#endif /* ! __USERPROG_KSYSCALL_H__ */
