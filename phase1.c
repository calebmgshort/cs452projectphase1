/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Fall 2015

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();


/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr ReadyList;

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
    int result; /* value returned by call to fork1() */

    /* initialize the process table */
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");

    for (int i = 0; i < MAXPROC; i++)
    {
        ProcTable[i].pid = 0;
    }

    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");
    ReadyList = NULL;

    // Initialize the clock interrupt handler

    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }
  
    // start the test process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }

    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");

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
int getNextPid();
int pidToSlot(int);

int fork1(char *name, int (*startFunc)(char *), char *arg, int stacksize, int priority)
{
    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

    // test if in kernel mode; halt if in user mode
    unsigned int kernalMode = USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE;
    if (!kernalMode)
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

    // fill-in entry in process table
    proc->nextProcPtr = NULL;
    proc->childProcPtr = NULL;
    proc->nextSiblingPtr = NULL;

    if (name == NULL)
    {
        if (DEBUG && debugflag)
            USLOSS_Console("fork1(): Process name cannot be null.\n");
        return -1;
    }
    if (strlen(name) >= (MAXNAME - 1)) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    strcpy(proc->name, name);

    if (arg == NULL)
        ProcTable[procSlot].startArg[0] = '\0';
    else if (strlen(arg) >= (MAXARG - 1)) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(proc->startArg, arg);
    
    proc->stack = malloc(sizeof(char) * stacksize);
    if (proc->stack == NULL)
    {
        USLOSS_Console("fork1(): Cannot allocate stack for process.  Halting...\n");
        USLOSS_Halt(1);
    }
    proc->stackSize = stacksize;
    
    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)
    USLOSS_ContextInit(&(proc->state),
                       proc->stack,
                       proc->stackSize,
                       NULL,
                       launch);

    proc->pid = pid;

    if (priority < 1 || priority > SENTINELPRIORITY)
    {
        if (DEBUG && debugflag)
            USLOSS_Console("fork1(): Priority out of range.\n");
        return -1;
    }
    proc->priority = priority;

    if (startFunc == NULL)
    {
        if (DEBUG && debugflag)
            USLOSS_Console("fork1(): Trying to start a process with no function.\n");
        return -1;
    }
    proc->startFunc = startFunc;

    proc->status = 0; // TODO define constant

    // for future phase(s)
    p1_fork(ProcTable[procSlot].pid);

    // Modify the ready list TODO

    // TODO Modify current?

    // Call the dispatcher
    dispatcher(); // TODO is the sentinel a special case?

    return pid;
} /* fork1 */

/*
 * Helper for fork1() that calculates the pid for the next process.
 * 
 * TODO clear out dead processes when space is needed.
 */
int getNextPid()
{
    int slot = pidToSlot(nextPid);
    procStruct proc = ProcTable[slot];

    if (proc.pid == 0)
    {
        return slot + 1; // this slot was never occupied, so this pid is new
    }

    // slot is taken, use linear probing to search for an open slot
    for (int i = 0; i < MAXPROC; i++)
    {
        proc = ProcTable[(slot + i) % MAXPROC];
        if (proc.pid == 0)
        {
            return ((slot + i) % MAXPROC) + 1; // same as above
        }
    }

    // TODO search for dead processes to remove

    return -1; // no space left in the table
}

/*
 * Helper for fork1() that hashes a pid into a table index.
 */
int pidToSlot(int pid)
{
    return (pid - 1) % MAXPROC;
}

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
    p1_quit(Current->pid);
} /* quit */


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
