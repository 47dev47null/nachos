#include "memmgr.h"
#include "synch.h"

MemoryManager::MemoryManager()
{
    lock = new Lock("MemoryManager Lock");
    flags = new Bitmap(NumPhysPages);
    used = 0;
}

MemoryManager::~MemoryManager()
{
    delete lock;
    delete flags;
}

// reservePages must be called before call getPage!
int
MemoryManager::getPage()
{
    lock->Acquire();
    int result = flags->FindAndSet();
    //TODO: what if result is -1, that is out of memory?
    ASSERT(result != -1);
    lock->Release();
}

void
MemoryManager::clearPage(int i)
{
    lock->Acquire();
    //TODO: what if clear a page which is out of range?
    if(!flags->Test(i))
        return;
    flags->Clear(i);
    used--;
    lock->Release();
}

int
MemoryManager::getFreePageCount()
{
    return NumPhysPages - used;
}

bool
MemoryManager::reservePages(int num)
{
    lock->Acquire();
    bool ok = (NumPhysPages - used >= num);
    if (ok)
        used += num;
    lock->Release();
    return ok;
}
