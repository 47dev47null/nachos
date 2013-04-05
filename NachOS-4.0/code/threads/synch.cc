// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables.
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Once we'e implemented one set of higher level atomic operations,
// we can implement others using that implementation.  We illustrate
// this by implementing locks and condition variables on top of 
// semaphores, instead of directly enabling and disabling interrupts.
//
// Locks are implemented using a semaphore to keep track of
// whether the lock is held or not -- a semaphore value of 0 means
// the lock is busy; a semaphore value of 1 means the lock is free.
//
// The implementation of condition variables using semaphores is
// a bit trickier, as explained below under Condition::Wait.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "main.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(char* debugName, int initialValue)
{
    name = debugName;
    value = initialValue;
    queue = new List<Thread *>;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
    Interrupt *interrupt = kernel->interrupt;
    Thread *currentThread = kernel->currentThread;
    
    // disable interrupts
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	
    
    while (value == 0) { 		// semaphore not available
	queue->Append(currentThread);	// so go to sleep
	currentThread->Sleep(FALSE);
    } 
    value--; 			// semaphore available, consume its value
   
    // re-enable interrupts
    (void) interrupt->SetLevel(oldLevel);	
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that interrupts
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
    Interrupt *interrupt = kernel->interrupt;
    
    // disable interrupts
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	
    
    if (!queue->IsEmpty()) {  // make thread ready.
	kernel->scheduler->ReadyToRun(queue->RemoveFront());
    }
    value++;
    
    // re-enable interrupts
    (void) interrupt->SetLevel(oldLevel);
}

//----------------------------------------------------------------------
// Semaphore::SelfTest, SelfTestHelper
// 	Test the semaphore implementation, by using a semaphore
//	to control two threads ping-ponging back and forth.
//----------------------------------------------------------------------

static Semaphore *ping;
static void
SelfTestHelper (Semaphore *pong) 
{
    for (int i = 0; i < 10; i++) {
        ping->P();
        cout << "pong get ping: " << i << "\n";
	    pong->V();
    }
}

void
Semaphore::SelfTest()
{
    Thread *helper = new Thread("ping");

    ASSERT(value == 0);		// otherwise test won't work!
    ping = new Semaphore("ping", 0);
    helper->Fork((VoidFunctionPtr) SelfTestHelper, this);
    for (int i = 0; i < 10; i++) {
        ping->V();
	    this->P();
        cout << "ping get pong: " << i << "\n";
    }
    delete ping;
}

//----------------------------------------------------------------------
// Lock::Lock
// 	Initialize a lock, so that it can be used for synchronization.
//	Initially, unlocked.
//
//	"debugName" is an arbitrary name, useful for debugging.
//----------------------------------------------------------------------

Lock::Lock(char* debugName)
{
    name = debugName;
    semaphore = new Semaphore("lock", 1);  // initially, unlocked
    lockHolder = NULL;
}

//----------------------------------------------------------------------
// Lock::~Lock
// 	Deallocate a lock
//----------------------------------------------------------------------
Lock::~Lock()
{
    delete semaphore;
}

//----------------------------------------------------------------------
// Lock::Acquire
//	Atomically wait until the lock is free, then set it to busy.
//	Equivalent to Semaphore::P(), with the semaphore value of 0
//	equal to busy, and semaphore value of 1 equal to free.
//----------------------------------------------------------------------

void Lock::Acquire()
{
    semaphore->P();
    lockHolder = kernel->currentThread;
}

//----------------------------------------------------------------------
// Lock::Release
//	Atomically set lock to be free, waking up a thread waiting
//	for the lock, if any.
//	Equivalent to Semaphore::V(), with the semaphore value of 0
//	equal to busy, and semaphore value of 1 equal to free.
//
//	By convention, only the thread that acquired the lock
// 	may release it.
//---------------------------------------------------------------------

void Lock::Release()
{
    ASSERT(IsHeldByCurrentThread());
    lockHolder = NULL;
    semaphore->V();
}

//----------------------------------------------------------------------
// Condition::Condition
// 	Initialize a condition variable, so that it can be 
//	used for synchronization.  Initially, no one is waiting
//	on the condition.
//
//	"debugName" is an arbitrary name, useful for debugging.
//----------------------------------------------------------------------
Condition::Condition(char* debugName)
{
    name = debugName;
    waitQueue = new List<Semaphore *>;
}

//----------------------------------------------------------------------
// Condition::Condition
// 	Deallocate the data structures implementing a condition variable.
//----------------------------------------------------------------------

Condition::~Condition()
{
    delete waitQueue;
}

//----------------------------------------------------------------------
// Condition::Wait
// 	Atomically release monitor lock and go to sleep.
//	Our implementation uses semaphores to implement this, by
//	allocating a semaphore for each waiting thread.  The signaller
//	will V() this semaphore, so there is no chance the waiter
//	will miss the signal, even though the lock is released before
//	calling P().
//
//	Note: we assume Mesa-style semantics, which means that the
//	waiter must re-acquire the monitor lock when waking up.
//
//	"conditionLock" -- lock protecting the use of this condition
//----------------------------------------------------------------------

void Condition::Wait(Lock* conditionLock) 
{
     Semaphore *waiter;
    
     ASSERT(conditionLock->IsHeldByCurrentThread());

     waiter = new Semaphore("condition", 0);
     waitQueue->Append(waiter);
     conditionLock->Release();
     waiter->P();
     conditionLock->Acquire();
     delete waiter;
}

//----------------------------------------------------------------------
// Condition::Signal
// 	Wake up a thread waiting on this condition, if any.
//
//	Note: we assume Mesa-style semantics, which means that the
//	signaller doesn't give up control immediately to the thread
//	being woken up (unlike Hoare-style).
//
//	Also note: we assume the caller holds the monitor lock
//	(unlike what is described in Birrell's paper).  This allows
//	us to access waitQueue without disabling interrupts.
//
//	"conditionLock" -- lock protecting the use of this condition
//----------------------------------------------------------------------

void Condition::Signal(Lock* conditionLock)
{
    Semaphore *waiter;
    
    ASSERT(conditionLock->IsHeldByCurrentThread());
    
    if (!waitQueue->IsEmpty()) {
        waiter = waitQueue->RemoveFront();
	waiter->V();
    }
}

//----------------------------------------------------------------------
// Condition::Broadcast
// 	Wake up all threads waiting on this condition, if any.
//
//	"conditionLock" -- lock protecting the use of this condition
//----------------------------------------------------------------------

void Condition::Broadcast(Lock* conditionLock) 
{
    while (!waitQueue->IsEmpty()) {
        Signal(conditionLock);
    }
}

//----------------------------------------------------------------------
// Elevator Test
//----------------------------------------------------------------------

static Lock *lock;
static Condition *elev;
static Condition **floors;
static int *in, *out;
static int s, dir, id;

static void ElevatorHelper(int n)
{
    lock->Acquire();

    while (true)
    {
        int i;
        for (i = 1; i <= n; i++)
            if (in[i] + out[i] > 0) 
                break;
        if (i > n)
            elev->Wait(lock);

        cout << "Elevator arrives on floor " << s << ".\n";

        if (in[s] + out[s] > 0)
        {
            floors[s]->Broadcast(lock);
            while (in[s] + out[s] > 0)
                elev->Wait(lock);
        }

        for (int i = 0; i < 100; i++)
            ;

        s += dir;

        if (s == 1 || s == n)
            dir *= -1;
    }

    lock->Release();
}

static void Elevator(int n)
{
    lock = new Lock("Elevator lock");
    elev = new Condition("Elevator condition");
    floors = new Condition* [n+1];
    in = new int [n+1];
    out = new int [n+1];
    for (int i = 1; i <= n; i++)
    {
        floors[i] = new Condition("floor");
        in[i] = out [i] = 0;
    }
    s = dir = id = 1;
    Thread *e = new Thread ("Elevator thread");
    e->Fork((VoidFunctionPtr) ElevatorHelper, (void *)n);
}

typedef struct
{
    int id;
    int f, t;
} People;

static void PeopleHelper(People *p)
{
    lock->Acquire();

    cout << "Person " << p->id << " wants to go to floor " << p->t << " from floor " << p->f << ".\n";
    
    in[p->f]++;
    elev->Signal(lock);
    floors[p->f]->Wait(lock);

    ASSERT(s == p->f);
    cout << "Person " << p->id << " got into the elevator.\n";
    out[p->t]++;
    in[p->f]--;
    elev->Signal(lock);
    floors[p->t]->Wait(lock);

    ASSERT(s == p->t);
    cout << "Person " << p->id << " got out of the elevator.\n";
    out[p->t]--;
    elev->Signal(lock);

    lock->Release();
}

static void FromTo(int f, int t)
{
    People *p = new People();
    p->id = id++;
    p->f = f;
    p->t = t;

    Thread *pt = new Thread("People thread");
    pt->Fork((VoidFunctionPtr) PeopleHelper, (void *)p);
}

static void CleanElevatorTest(int n)
{
    lock->Acquire();
    delete elev;
    delete []in;
    delete []out;
    for (int i = 1; i <= n; i++)
    {
        delete floors[i];
    }
    delete []floors;
    lock->Release();
    delete lock;
}


void ElevatorTest()
{
    cout << "Elevator starts.\n";
    Elevator(5);
    FromTo(1, 4);
    FromTo(3, 1);
    FromTo(2, 4);
    FromTo(5, 1);
    int i;
    while (true)
    {
        for (i = 1; i <= 5; i++)
            if (in[i] + out[i] > 0) 
                break;
        if (i > 5)
            break;
        kernel->currentThread->Yield();
    }
    cout << "Elevator halts.\n";
    CleanElevatorTest(5);
}
