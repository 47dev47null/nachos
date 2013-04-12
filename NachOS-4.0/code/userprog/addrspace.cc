// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -n -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you are using the "stub" file system, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "addrspace.h"
#include "machine.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
#ifdef RDATA
    noffH->readonlyData.size = WordToHost(noffH->readonlyData.size);
    noffH->readonlyData.virtualAddr = 
           WordToHost(noffH->readonlyData.virtualAddr);
    noffH->readonlyData.inFileAddr = 
           WordToHost(noffH->readonlyData.inFileAddr);
#endif 
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);

#ifdef RDATA
    DEBUG(dbgAddr, "code = " << noffH->code.size <<  
                   " readonly = " << noffH->readonlyData.size <<
                   " init = " << noffH->initData.size <<
                   " uninit = " << noffH->uninitData.size << "\n");
#endif
}

// read / write from kernel to user
int 
AddrSpace::userReadWrite(char *kspace, int virtAddr, int size, int wflag)
{
    unsigned int vpn = (unsigned) virtAddr / PageSize;
    int offset = virtAddr % PageSize;
    // handle cross pages
    int copySize = min(size, PageSize - offset);
    while (size)
    {
        int pa = pageTable[vpn].physicalPage * PageSize + offset;
        if (wflag)
            bcopy(kspace, kernel->machine->mainMemory + pa, copySize);
        else
            bcopy(kernel->machine->mainMemory + pa, kspace, copySize);
        size -= copySize;
        kspace += copySize;
        vpn++;
        offset = 0;
        copySize = min(size, PageSize);
    }
    return 1;
}

// read <size> bytes from File <file> starting at <inFileAddr> to <virtAddr>
// use kernel diskBuffer as an internel cache
void
AddrSpace::ReadFile(int virtAddr, OpenFile *file, int size, int inFileAddr)
{
    int readSize;
    while (size)
    {
        readSize = min(PageSize, size);
        file->ReadAt(kernel->diskBuffer, PageSize, inFileAddr);
        userReadWrite(kernel->diskBuffer, virtAddr, readSize, 1);
        inFileAddr += readSize;
        virtAddr += readSize;
        size -= readSize;
    }
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//----------------------------------------------------------------------

AddrSpace::AddrSpace()
{
    // pageTable is installed in Load or Fork
    InitProc();
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    // free memory pages this process occupys
    if (pageTable != NULL)
    {
        for (int i = 0; i < numPages; i++)
            kernel->memmgr->clearPage(pageTable[i].physicalPage);
        delete pageTable;
    }
}


//----------------------------------------------------------------------
// AddrSpace::Load
// 	Load a user program into memory from a file.
//
//	Assumes that the page table has been initialized, and that
//	the object code file is in NOFF format.
//
//	"fileName" is the file containing the object code to load into memory
//----------------------------------------------------------------------

bool 
AddrSpace::Load(char *fileName) 
{
    // drop original pageTable and free corresponding memory (if any)
    if (pageTable != NULL)
    {
        for (int i = 0; i < numPages; i++)
            kernel->memmgr->clearPage(pageTable[i].physicalPage);
        delete pageTable;
    }

    OpenFile *executable = kernel->fileSystem->Open(fileName);
    NoffHeader noffH;
    unsigned int size;

    if (executable == NULL) {
        cerr << "Unable to open file " << fileName << "\n";
        return FALSE;
    }

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

#ifdef RDATA
// how big is address space?
    size = noffH.code.size + noffH.readonlyData.size + noffH.initData.size +
           noffH.uninitData.size + UserStackSize;	
                                                // we need to increase the size
						// to leave room for the stack
#else
// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
#endif
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory

    DEBUG(dbgAddr, "Initializing address space: " << numPages << ", " << size);

    pageTable = new TranslationEntry[numPages];
    ASSERT(kernel->memmgr->reservePages(numPages));
    for (int i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;
        int ppn = kernel->memmgr->getPage();
        pageTable[i].physicalPage = ppn;
        DEBUG(dbgAddr, "[Page Table]: vpn " << i << " ppn " << ppn );
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;
        bzero(kernel->machine->mainMemory + ppn * PageSize, PageSize);
    }


// then, copy in the code and data segments into memory
    if (noffH.code.size > 0) {
        DEBUG(dbgAddr, "Initializing code segment.");

	    DEBUG(dbgAddr, "VA: " << noffH.code.virtualAddr << ", "
                << "Size: " << noffH.code.size);
 
        ReadFile(noffH.code.virtualAddr, executable, 
                noffH.code.size, noffH.code.inFileAddr);
    }

    if (noffH.initData.size > 0) {
        DEBUG(dbgAddr, "Initializing data segment.");

	    DEBUG(dbgAddr, "VA: " << noffH.initData.virtualAddr << ", "
                << "Size: " << noffH.initData.size);

        ReadFile(noffH.initData.virtualAddr, executable,
                noffH.initData.size, noffH.initData.inFileAddr);
    }

#ifdef RDATA
    if (noffH.readonlyData.size > 0) {
        DEBUG(dbgAddr, "Initializing read only data segment.");

	    DEBUG(dbgAddr, "VA: " << noffH.readonlyData.virtualAddr << ", "
                "Size: " << noffH.readonlyData.size);

        ReadFile(noffH.readonlyData.virtualAddr, executable, 
                noffH.readonlyData.size, noffH.readonlyData.inFileAddr);
    }
#endif

    delete executable;			// close file
    return TRUE;			// success
}

//----------------------------------------------------------------------
// AddrSpace::Execute
// 	Run a user program using the current thread
//
//      The program is assumed to have already been loaded into
//      the address space
//
//----------------------------------------------------------------------

void 
AddrSpace::Execute() 
{

    kernel->currentThread->space = this;

    this->InitRegisters();		// set the initial register values
    this->RestoreState();		// load page table register

    kernel->machine->Run();		// jump to the user progam

    ASSERTNOTREACHED();			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}


//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    Machine *machine = kernel->machine;
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start", which
    //  is assumed to be virtual address zero
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    // Since instructions occupy four bytes each, the next instruction
    // after start will be at virtual address four.
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG(dbgAddr, "Initializing stack pointer: " << numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, don't need to save anything!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    kernel->machine->pageTable = pageTable;
    kernel->machine->pageTableSize = numPages;
}


//----------------------------------------------------------------------
// AddrSpace::Translate
//  Translate the virtual address in _vaddr_ to a physical address
//  and store the physical address in _paddr_.
//  The flag _isReadWrite_ is false (0) for read-only access; true (1)
//  for read-write access.
//  Return any exceptions caused by the address translation.
//----------------------------------------------------------------------
ExceptionType
AddrSpace::Translate(unsigned int vaddr, unsigned int *paddr, int isReadWrite)
{
    TranslationEntry *pte;
    int               pfn;
    unsigned int      vpn    = vaddr / PageSize;
    unsigned int      offset = vaddr % PageSize;

    if(vpn >= numPages) {
        return AddressErrorException;
    }

    pte = &pageTable[vpn];

    if(isReadWrite && pte->readOnly) {
        return ReadOnlyException;
    }

    pfn = pte->physicalPage;

    // if the pageFrame is too big, there is something really wrong!
    // An invalid translation was loaded into the page table or TLB.
    if (pfn >= NumPhysPages) {
        DEBUG(dbgAddr, "Illegal physical page " << pfn);
        return BusErrorException;
    }

    pte->use = TRUE;          // set the use, dirty bits

    if(isReadWrite)
        pte->dirty = TRUE;

    *paddr = pfn*PageSize + offset;

    ASSERT((*paddr < MemorySize));

    //cerr << " -- AddrSpace::Translate(): vaddr: " << vaddr <<
    //  ", paddr: " << *paddr << "\n";

    return NoException;
}

void
AddrSpace::InitProc()
{
    proc = new Proc;
    // get new process id
    proc->pid = kernel->procmgr->getPID();
    // set parent id to 0, which will be updated in Fork
    proc->ppid = 0;
    proc->thread = kernel->currentThread;
    proc->joinNum = -1;
    proc->alive = true;
    proc->retValue = 0;
    // add proc to list
    kernel->procmgr->procs[proc->pid] = proc;
}

AddrSpace*                                                                      
AddrSpace::Fork()                                                               
{                                                                               
    AddrSpace *dup = new AddrSpace();                                             
    ASSERT(proc == kernel->currentThread->space->proc);
    dup->proc->ppid = proc->pid;                                                
    dup->numPages = numPages;                                                   
    dup->pageTable = new TranslationEntry[numPages];
    // copy page table                                                          
    ASSERT(kernel->memmgr->reservePages(numPages));

    DEBUG(dbgAddr, "Forking address space: " << numPages << " pages.");

    unsigned int src, dest;
    for (int i = 0; i < numPages; i++)                                          
    {                                                                           
        dup->pageTable[i].virtualPage = i;               
        int ppn = kernel->memmgr->getPage();
        dup->pageTable[i].physicalPage = ppn;             
        DEBUG(dbgAddr, "[Page Table]: vpn " << i << " ppn " << ppn);
        dup->pageTable[i].valid = pageTable[i].valid;                           
        dup->pageTable[i].use = pageTable[i].use;                               
        dup->pageTable[i].dirty = pageTable[i].dirty;                           
        dup->pageTable[i].readOnly = pageTable[i].readOnly;

        src = pageTable[i].physicalPage * PageSize;
        dest = dup->pageTable[i].physicalPage * PageSize;
        bcopy(kernel->machine->mainMemory + src,
                kernel->machine->mainMemory + dest, PageSize);
    }                                                                           

    return dup;                                                                 
}
