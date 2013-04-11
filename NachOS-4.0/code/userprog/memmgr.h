#ifndef __USERPORG_MEMMGR_H__
#define __USERPORG_MEMMGR_H__
#include "bitmap.h"
#include "machine.h"

class Lock;

class MemoryManager
{
    public:
        MemoryManager();
        ~MemoryManager();
        int getPage();
        void clearPage(int i);
        int getFreePageCount();
        bool reservePages(int num);
    private:
        Bitmap *flags;
        int used;       // the number of used pages
        Lock *lock;
};

#endif
