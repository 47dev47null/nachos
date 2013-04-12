/**************************************************************
 *
 * userprog/ksyscall.cc
 *
 * Kernel interface for systemcalls 
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#include "ksyscall.h"
#include "synch.h"
#include "errno.h"

// ReadStr
// read string from virtAddr to buf
// stop when meet '\0' or size characters readed.
// return number of characters readed, including '\0'
static int ReadStr(int virtAddr, char *buf, unsigned size)
{
    if (size <= 0)
        return -1;

    DEBUG(dbgSys, "read string from VA: " << virtAddr << " to kernel buffer.");

    char *sp = buf;
    do {
        kernel->machine->ReadMem(virtAddr++, sizeof(char), (int *)sp);
    } while (*sp++ != '\0' && sp < buf + size);
    DEBUG(dbgSys, "read string: " << buf);
    return sp - buf;
}

// WriteStr
// write string from buf to virtAddr
// stop when meet '\0' or size characters readed.
// return number of characters readed, including '\0'
static int WriteStr(int virtAddr, char *buf, unsigned size)
{
    if (size <= 0)
        return -1;

    DEBUG(dbgSys, "write string from kernel buffer to VA: " << virtAddr);

    char *sp = buf;
    do {
        kernel->machine->WriteMem(virtAddr++, sizeof(char), (int)*sp);
    } while (*sp++ != '\0' && sp < buf + size);
    DEBUG(dbgSys, "write string: " << buf);     // Unreliable debug info!
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
    char *kname = new char[MAX_ARG_LEN];
    if (ReadStr(uname, kname, MAX_ARG_LEN) == -1)
        return 1;
    if (kernel->currentThread->space->Load(kname) == FALSE)
        return 1;
    delete []kname;
    DEBUG(dbgSys, "[System Call] New Executable Loaded.");
    kernel->currentThread->space->Execute();
    ASSERTNOTREACHED();
    return 0;
}

int SysExecV(int argc, int argv)
{
    // get progname, and store it in kprogname
    int uprogname;
    char *kprogname = new char[MAX_ARG_LEN];
    if (kernel->machine->ReadMem(argv, sizeof(char *), &uprogname) == FALSE)
        return 1;
    if (ReadStr(uprogname, kprogname, MAX_ARG_LEN) == -1)
        return 1;
    DEBUG(dbgSys, "[System Call] ProgName: " << kprogname);

    // get args, and store them in kargv
    int uargv;
    char **kargv = new char*[argc];
    for (int i = 0; i < argc; i++)
    {
        kargv[i] = new char[MAX_ARG_LEN];
        if (kernel->machine->ReadMem(argv + i * sizeof(char *), sizeof(char *),
                    &uargv) == FALSE)
            return 1;
        if (ReadStr(uargv, kargv[i], MAX_ARG_LEN) == -1)
            return 1;
        DEBUG(dbgSys, "[System Call] Arg " << i << ": " << kargv[i]);
    }

    if (kernel->currentThread->space->Load(kprogname) == FALSE)
        return 1;
    DEBUG(dbgSys, "[System Call] Program " << kprogname << " Loaded.");

    // set up stack
    int stackBottom;
    int argHead;

    stackBottom = kernel->machine->ReadRegister(StackReg)
        + 16 - UserStackSize + 16;

    argHead = stackBottom + sizeof(char *) * argc;

    for (int i = 0; i < argc; i++)
    {
        if (kernel->machine->WriteMem(stackBottom + i * sizeof(char *),
                sizeof(char *), argHead) == FALSE)
            return 1;
        int len = WriteStr(argHead, kargv[i], MAX_ARG_LEN);
        if (len == -1)
            return 1;
        argHead += len;

        delete []kargv[i];
    }
    delete []kargv;
    delete []kprogname;

    // since we need to pass arguments,
    // we do not use AddrSpace::Execute directly.
    kernel->currentThread->space->InitRegisters();
    kernel->currentThread->space->RestoreState();
    // pass arguements
    kernel->machine->WriteRegister(4, argc);
    kernel->machine->WriteRegister(5, stackBottom);

    kernel->machine->Run();

    ASSERTNOTREACHED();
    return 1;
}

int SysJoin(int pid)
{
    if (kernel->procmgr->validPID(pid) == FALSE)
    {
        DEBUG(dbgSys, "[System Call] Join a invalid pid: " << pid);
        return -1;
    }
    Proc *proc;
    if ((proc = kernel->procmgr->procs[pid]) == NULL)
    {
        DEBUG(dbgSys, "[System Call] Join a unexisted pid: " << pid);
        return -1;
    }

    if (proc->alive == FALSE)       // the proc already exits
        return proc->retValue;

    Lock *procLock = kernel->procmgr->getLock(pid);
    procLock->Acquire();

    proc->joinNum++;                // regiser into join list
    kernel->procmgr->getCondition(pid)->Wait(procLock);

    proc->joinNum--;
    ASSERT(proc->alive == FALSE);
    int retValue = proc->retValue;

    procLock->Release();

    return retValue;
}

void SysExit(int rc)
{
    DEBUG(dbgSys, "[System Call] Exit Handler get called.");
}
