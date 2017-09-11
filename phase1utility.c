/* ------------------------------------------------------------------------
   phase1utility.c
   Utility file for phase1.c

   University of Arizona
   Computer Science 452
   Fall 2017

   ------------------------------------------------------------------------ */

#include "phase1utility.h"

extern unsigned int nextPid;
extern procStruct ProcTable[];
extern int debugflag;

void launch();


/*
 * Helper for fork1() that calculates the pid for the next process.
 *
 * TODO clear out dead processes when space is needed.
 */
int getNextPid()
{
    int slot = pidToSlot(nextPid);
    procPtr proc = &ProcTable[slot];

    if (!processExists(proc))
    {
        return slot; // this slot was never occupied, so this pid is new
    }

    // slot is taken, use linear probing to search for an open slot
    for (int i = 0; i < MAXPROC; i++)
    {
        proc = &ProcTable[(slot + i) % MAXPROC];
        if (!processExists(proc))
        {
            return ((slot + i) % MAXPROC); // same as above
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
    return (pid) % MAXPROC;
}

bool processExists(procPtr process)
{
    if(process->pid == 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool inKernelMode()
{
    return (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) > 0;
}

/*
 * Helper for fork1() that fills out all of the fields in the procStruct that
 * proc points to. pid should be the pid of the new process. See fork1() for
 * all other parameters.
 *
 * Returns -1 iff one of the parameters is invalid in such a way that fork1()
 * should return -1. Returns 0 otherwise.
 */
int initProc(procPtr parentPtr, procPtr proc, char *name, int (*startFunc)(char *), char *arg, int stacksize, int priority, int pid)
{
    // fill out list pointers
    proc->nextProcPtr = NULL;
    proc->childProcPtr = NULL;
    proc->nextSiblingPtr = NULL;
    proc->quitChildPtr = NULL;
    proc->nextQuitSiblingPtr = NULL;
    proc->parentPtr = parentPtr;

    // fill out name
    if (name == NULL)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("fork1(): Process name cannot be null.\n");
        }
        return -1;
    }
    if (strlen(name) >= (MAXNAME - 1))
    {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    strcpy(proc->name, name);

    // fill out argument
    if (arg == NULL)
    {
        proc->startArg[0] = '\0';
    }
    else if (strlen(arg) >= (MAXARG - 1))
    {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
    {
        strcpy(proc->startArg, arg);
    }

    // create the stack
    proc->stack = malloc(sizeof(char) * stacksize);
    if (proc->stack == NULL)
    {
        USLOSS_Console("fork1(): Cannot allocate stack for process.  Halting...\n");
        USLOSS_Halt(1);
    }
    proc->stackSize = stacksize;

    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)
    USLOSS_ContextInit(&(proc->state), proc->stack, proc->stackSize, NULL, launch);

    // fill out priority
    if (priority < 1 || priority > SENTINELPRIORITY)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("fork1(): Priority out of range.\n");
        }
        return -1;
    }
    proc->priority = priority;

    // fill out startFunc
    if (startFunc == NULL)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("fork1(): Trying to start a process with no function.\n");
        }
        return -1;
    }
    proc->startFunc = startFunc;

    // fill out the rest of the fields
    proc->pid = pid;
    proc->status = STATUS_READY;

    return 0;
}

/*
 * Checks the current mode and halts if in user mode.
 */
void checkMode(char * funcName)
{
    // test if in kernel mode; halt if in user mode
    if (!inKernelMode())
    {
        USLOSS_Console("%s(): Called in user mode.  Halting...\n", funcName);
        USLOSS_Halt(1);
    }
}

/*
 * Returns the number of children in the given process
 */
int numChildren(procPtr process)
{
    int numChildren = 0;
    procPtr currentChildPtr = process->childProcPtr;
    while(currentChildPtr != NULL)
    {
        numChildren++;
        currentChildPtr = currentChildPtr->nextProcPtr;
    }
    return numChildren;
}

/*
 * Disables interrupts.
 */
void disableInterrupts()
{
    // check for kernel mode
    checkMode("disableInterrupts");

    // get the current value of the psr
    unsigned int psr = USLOSS_PsrGet();

    // the current interrupt bit of the psr
    unsigned int currentInterrupt = psr & USLOSS_PSR_CURRENT_INT;

    // set the prev interrupt bit to zero
    psr = psr & ~USLOSS_PSR_PREV_INT;

    // move the current interrupt bit to the prev interrupt bit
    psr = psr | (currentInterrupt << 2);

    // set the current interrupt bit to 0
    psr = psr & ~USLOSS_PSR_CURRENT_INT;

    /*
    // if USLOSS gives an error, we've done something wrong!
    if (USLOSS_PsrSet(psr) == USLOSS_ERR_INVALID_PSR)
    {
        if (DEBUG && debugflag)
            USLOSS_Console("launch(): Bug in interrupt set.");
    }
    */
} /* disableInterrupts */

/*
 * Enables interrupts.
 */
void enableInterrupts()
{
    // check for interrupts
    checkMode("enableInterrupts");

    // get the current value of the psr
    unsigned int psr = USLOSS_PsrGet();

    // the current interrupt bit of the psr
    unsigned int currentInterrupt = psr & USLOSS_PSR_CURRENT_INT;

    // set the prev interrupt bit to zero
    psr = psr & ~USLOSS_PSR_PREV_INT;

    // move the current interrupt bit to the prev interrupt bit
    psr = psr | (currentInterrupt << 2);

    // set the current interrupt bit to 1
    psr = psr | USLOSS_PSR_CURRENT_INT;

    /*
    // if USLOSS gives an error, we've done something wrong!
    if (USLOSS_PsrSet(psr) == USLOSS_ERR_INVALID_PSR)
    {
        if (DEBUG && debugflag)
            USLOSS_Console("launch(): Bug in interrupt set.");
    }
    */
} /* enableInterrupts */
