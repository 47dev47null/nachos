#ifndef __USERPORG_PROCMGR_H__
#define __USERPORG_PROCMGR_H__
#include "bitmap.h"

#define MaxNumProcesses 200

class Proc;
class Lock;
class Condition;

class ProcessManager
{
    public:
        ProcessManager();
        ~ProcessManager();
        int getPID();
        void clearPID(int pid);
        Lock* getLock(int pid);
        Condition* getCondition(int pid);
        Proc *procs[MaxNumProcesses];
        bool validPID(int pid);
    private:
        Bitmap *flags;
        Lock *lock;
        Lock *locks[MaxNumProcesses];
        Condition *conds[MaxNumProcesses];
};

#endif
