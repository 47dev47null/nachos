#include "procmgr.h"
#include "synch.h"

ProcessManager::ProcessManager()
{
    flags = new Bitmap(MaxNumProcesses);
    lock = new Lock("procmgr");
}

ProcessManager::~ProcessManager()
{
    for (int i=0; i<MaxNumProcesses; i++)
    { 
        if (procs[i]!=NULL) delete procs[i];
        if (locks[i]!=NULL) delete locks[i];
        if (conds[i]!=NULL) delete conds[i]; 
    }
    delete flags;
    delete lock;
}

bool
ProcessManager::validPID(int pid)
{
    return 0 <= pid && pid < MaxNumProcesses;
}

int
ProcessManager::getPID()
{
    int pid;
    lock->Acquire();
    pid = flags->FindAndSet();
    if (pid == -1)
    {
        for (int i = 0; i < MaxNumProcesses; i++)
        {
            if (procs[i] && !procs[i]->alive && procs[i]->joinNum == -1)
            {
                delete procs[i];
                procs[i] = NULL;
                flags->Clear(i);
            }
        }
        pid = flags->FindAndSet();
    }
    lock->Release();
    return pid;
}

void
ProcessManager::clearPID(int pid)
{
    lock->Acquire();
    flags->Clear(pid);
    lock->Release();
}

Lock*
ProcessManager::getLock(int pid)
{
    ASSERT(validPID(pid));
    if (locks[pid] == NULL)
        locks[pid] = new Lock("proc lock");
    return locks[pid];
}

Condition*
ProcessManager::getCondition(int pid)
{
    ASSERT(validPID(pid));
    if (conds[pid] == NULL)
        conds[pid] = new Condition("proc condition");
    return conds[pid];
}
