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

#define MAX_ARG_LEN 20

class Proc;
class Lock;
class Condition;

void SysHalt();

int SysAdd(int op1, int op2);

int SysFork();

int SysExec(int uname);

int SysExecV(int argc, int argv);

int SysJoin(int pid);

void SysExit(int rc);

int SysCreate(int uname);

int SysRemove(int uname);

#endif /* ! __USERPROG_KSYSCALL_H__ */
