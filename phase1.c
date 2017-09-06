/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Fall 2015

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include "kernel.h"
#include "phase1utility.h"
#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------- Prototypes ----------------------------------- */
void startup(int, char * []);
void finish(int, char * []);
void launch();
int sentinel (char *);
extern int start1 (char *);
static void checkDeadlock();
void disableInterrupts();


/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static priorityQueue ReadyList;

// current process ID
procPtr Current;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;

/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - argc and argv passed in by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup(int argc, char *argv[])
{
    int result; // value returned by call to fork1()

    // initialize the process table
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");

    for (int i = 0; i < MAXPROC; i++)
    {
        ProcTable[i].pid = 0;
    }

    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");
    initPriorityQueue(&ReadyList);
    Current = NULL;

    // Initialize the clock interrupt handler TODO

    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, SENTINELPRIORITY);
    if (result < 0)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("startup(): fork1 of sentinel returned error.  ");
            USLOSS_Console("Halting...\n");
        }
        USLOSS_Halt(1);
    }

    // start the test process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("startup(): fork1 for start1 returned an error.  ");
            USLOSS_Console("Halting...\n");
        }
        USLOSS_Halt(1);
    }

    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1.\n");

    return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish(int argc, char *argv[])
{
    if (DEBUG && debugflag)
        USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg, int stacksize, int priority)
{
    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

    // test if in kernel mode; halt if in user mode
    if (!inKernelMode())
    {
        USLOSS_Console("fork1(): Fork called in user mode.  Halting...\n");
        USLOSS_Halt(1);
    }

    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK)
    {
        if (DEBUG && debugflag)
            USLOSS_Console("fork1(): Fork called with stack size < USLOSS_MIN_STACK.\n");
        return -2;
    }

    // Is there room in the process table? What is the next PID?
    int pid = getNextPid();
    if (pid == -1)
    {
        if (DEBUG && debugflag)
            USLOSS_Console("fork1(): No room in the process table.\n");
        return -1;
    }
    nextPid = pid + 1;

    int procSlot = pidToSlot(pid);
    procPtr proc = &ProcTable[procSlot];

    // fill out Current's children pointers
    if (Current != NULL)
    {
        if (Current->childProcPtr == NULL)
        {
            Current->childProcPtr = proc;
        }
        else
        {
            // Current has children, look for youngest older sibling
            procPtr olderSib = Current->childProcPtr;
            while (olderSib->nextSiblingPtr != NULL)
            {
                olderSib = olderSib->nextSiblingPtr;
            }
            olderSib->nextSiblingPtr = proc;
        }
    }

    // fill out entry in process table
    if (initProc(proc, name, startFunc, arg, stacksize, priority, pid) == -1)
    {
        return -1;
    }

    // for future phase(s)
    p1_fork(proc->pid);

    // Modify the ready list
    addProc(ReadyList, proc);

    // Call the dispatcher
    if (priority != SENTINELPRIORITY)
    {
        dispatcher();
    }
    return pid;
} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
    int result;

    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

    // Enable interrupts
    unsigned int psr = USLOSS_PsrGet();
    unsigned int currentInterrupt = psr & USLOSS_PSR_CURRENT_INT;
    psr = psr & ~USLOSS_PSR_PREV_INT; // disregard old prev bit
    psr = psr | (currentInterrupt << 2); // move current int into prev int
    psr = psr | USLOSS_PSR_CURRENT_INT; // turn on interrupts
    if (USLOSS_PsrSet(psr) == USLOSS_ERR_INVALID_PSR)
    {
        if (DEBUG && debugflag)
            USLOSS_Console("launch(): Bug in interrupt set.");
    }

    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
        USLOSS_Console("Process %d returned to launch\n", Current->pid);

    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
    return -1;  // -1 is not correct! Here to prevent warning.
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{
    // check for any non quit children
    procPtr proc = Current->childProcPtr;
    while(proc != NULL)
    {
        if (proc->status != STATUS_QUIT)
        {
            USLOSS_Console("quit(): Process attempting to quit with living children.  Halting...\n");
            USLOSS_Halt(1);
        }
        proc = proc->nextSiblingPtr;
    }
    Current->status = STATUS_QUIT;
    // TODO take this process of the ready list
    // TODO notify parent of process death

    p1_quit(Current->pid);
} /* quit */

/* ------------------------------------------------------------------------
   Name - zap
   Purpose - Zaps a process with the given process id
   Parameters - the process id of the process to zap
   Returns - -1: the calling process itself was zapped while in zap.
              0: the zapped process has called quit.
   Side Effects - Forces another process to quit
   ------------------------------------------------------------------------ */
int zap(int pid){
  if(pid < 0){
    USLOSS_Console("zap failed because the given pid was out of range of the process table\n");
    USLOSS_Halt(1);
  }
  else if(!processExists(ProcTable[pidToSlot(pid)])){
    USLOSS_Console("zap failed because the given process does not exist\n");
    USLOSS_Halt(1);
  }
  else if(ProcTable[pidToSlot(pid)].pid == Current->pid){
    USLOSS_Console("zap failed because the given process to zap is itself\n");
    USLOSS_Halt(1);
  }
  else if(ProcTable[pidToSlot(pid)].status == STATUS_QUIT){
    USLOSS_Console("zap failed because the given process has already quit\n");
    USLOSS_Halt(1);
  }

  ProcTable[pid].status = STATUS_ZAPPED;
  while(ProcTable[pid].status != STATUS_QUIT){
    if(Current->status == STATUS_ZAPPED)
      return -1;
  }
  return 0;
}

/* ------------------------------------------------------------------------
   Name - isZapped
   Purpose - Determines if the current process has been zapped
   Parameters - none
   Returns - 0 - the current process has not been zapped
             1 - the current process has been zapped
   Side Effects - none
   ------------------------------------------------------------------------ */
int isZapped(void){
  if(Current->status == STATUS_ZAPPED)
    return 1;
  else
    return 0;
}

/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
    procPtr nextProcess = NULL;

    p1_switch(Current->pid, nextProcess->pid);
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */

int sentinel (char *dummy)
{
    if (DEBUG && debugflag)
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
} /* checkDeadlock */


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS

} /* disableInterrupts */
