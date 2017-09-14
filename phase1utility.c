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
extern priorityQueue ReadyList;
extern procPtr Current;

void launch();

/*
 * Helper for fork1() that calculates the pid for the next process.
 */
int getNextPid()
{
    int baseSlot = pidToSlot(nextPid);
    procPtr proc;
    int slot;

    // Use linear probing to search for an unused open slot
    for (int i = 0; i < MAXPROC; i++)
    {
        slot = (baseSlot + i) % MAXPROC;
        proc = &ProcTable[slot];
        if (proc->pid == PID_NEVER_EXISTED)
        {
            // this slot was never occupied, so this pid is new
            if (slot == 0)
            {
                return MAXPROC;
            }
            return slot;
        }

    }

    // Search for dead processes to remove
    for (int i = 0; i < MAXPROC; i++)
    {
        slot = (baseSlot + i) % MAXPROC;
        proc = &ProcTable[slot];
        if (proc->status == STATUS_DEAD)
        {
            // return the next pid that will hash to the same spot.
            return proc->pid + MAXPROC;
        }
    }

    return -1; // no space left in the table
}

/*
 * Helper for fork1() that hashes a pid into a table index.
 */
int pidToSlot(int pid)
{
    return pid % MAXPROC;
}

/*
 * Returns true iff process points to a used slot in the Process Table.
 */
bool processExists(procPtr process)
{
    return process->pid != PID_NEVER_EXISTED && process->status != STATUS_DEAD;
}

/*
 * Returns true iff we are currently in kernel mode.
 */
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
    proc->CPUTime = 0;

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
        USLOSS_Console("%s(): called while in user mode, by process %d. Halting...\n", funcName, Current->pid);
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
        currentChildPtr = currentChildPtr->nextSiblingPtr;
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



    // if USLOSS gives an error, we've done something wrong!
    if (USLOSS_PsrSet(psr) == USLOSS_ERR_INVALID_PSR)
    {
        if (DEBUG && debugflag)
            USLOSS_Console("launch(): Bug in disableInterrupts.");
    }

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


    // if USLOSS gives an error, we've done something wrong!
    if (USLOSS_PsrSet(psr) == USLOSS_ERR_INVALID_PSR)
    {
        if (DEBUG && debugflag)
            USLOSS_Console("launch(): Bug in enableInterrupts.");
    }

} /* enableInterrupts */

/*
 * Used by fork1 to add a child to the given process's child list
 */
void addChild(procPtr child, procPtr parent)
{
    if (parent != NULL)
    {
        if (parent->childProcPtr == NULL)
        {
            parent->childProcPtr = child;
            //USLOSS_Console("addChild(): after: childProcPtr=%d\n", child->pid);
        }
        else
        {
            // Current has children, look for youngest older sibling
            procPtr olderSib = parent->childProcPtr;
            while (olderSib->nextSiblingPtr != NULL)
            {
                olderSib = olderSib->nextSiblingPtr;
            }
            /*
            if(child->pid == 53)
            {
                USLOSS_Console("addChild(): olderSibling pid=%d, child pid=53.\n", olderSib->pid);
                procPtr bigBro = parent->childProcPtr;;
                while (bigBro->nextSiblingPtr != NULL)
                {
                    USLOSS_Console("pid=%d.\n", bigBro->pid);
                    bigBro = bigBro->nextSiblingPtr;
                }
                USLOSS_Console("pid=%d.\n", bigBro->pid);
            }
            */
            olderSib->nextSiblingPtr = child;
        }
    }
}

/*
 * Used by quit to add a child to the given process's quit child list
 */
void addQuitChild(procPtr parent, procPtr child)
{
    if(parent->quitChildPtr == NULL)
    {
        parent->quitChildPtr = child;
    }
    else
    {
        procPtr quitChildPtr = parent->quitChildPtr;
        // add Current to nextQuitSiblingPtr list
        while(quitChildPtr->nextQuitSiblingPtr != NULL)
        {
            quitChildPtr = quitChildPtr->nextQuitSiblingPtr;
        }
        quitChildPtr->nextQuitSiblingPtr = child;
    }
}

/*
 * Helper used by join to remove children from the child list.
 */
void removeDeadChildren(procPtr parent)
{
    if (parent == NULL)
    {
        return;
    }

    // Handle leading dead children
    while (parent->childProcPtr != NULL && parent->childProcPtr->status == STATUS_DEAD)
    {
        parent->childProcPtr = parent->childProcPtr->nextSiblingPtr;
    }

    procPtr olderSibling = parent->childProcPtr; // Older sibling is not dead
    while(olderSibling != NULL && olderSibling->nextSiblingPtr != NULL)
    {
        if (olderSibling->nextSiblingPtr->status == STATUS_DEAD)
        {
            olderSibling->nextSiblingPtr = olderSibling->nextSiblingPtr->nextSiblingPtr;
        }
        olderSibling = olderSibling->nextSiblingPtr;
    }
}

/*
 * Used by zap to add the zapping process to the list of processes that zapped
 * the zapped process
 */
void addZappedProcess(procPtr processZapping, procPtr processBeingZapped)
{
  processBeingZapped->isZapped = 1;
  if(processBeingZapped->procThatZappedMe == NULL)
  {
      processBeingZapped->procThatZappedMe = processZapping;
  }
  else
  {
      procPtr procThatZapped = processBeingZapped->procThatZappedMe;
      while(procThatZapped->nextSiblingThatZapped != NULL)
      {
          procThatZapped = procThatZapped->nextSiblingThatZapped;
      }
      procThatZapped->nextSiblingThatZapped = processZapping;
  }
}

void unblockProcessesThatZappedThisProcess(procPtr process)
{
  if(process->procThatZappedMe != NULL)
  {
      procPtr procThatZappedMe = process->procThatZappedMe;
      // Set each of these processes to ready
      while(procThatZappedMe != NULL)
      {
          procThatZappedMe->status = STATUS_READY;
          addProc(&ReadyList, procThatZappedMe);
          procThatZappedMe = procThatZappedMe->nextSiblingThatZapped;
      }
      // Remove the pointer to nextSiblingThatZapped for each
      procThatZappedMe = process->procThatZappedMe;
      procPtr last = procThatZappedMe;
      procThatZappedMe = procThatZappedMe->nextSiblingThatZapped;
      while(procThatZappedMe != NULL)
      {
          last->nextSiblingThatZapped = NULL;
          last = procThatZappedMe;
          procThatZappedMe = procThatZappedMe->nextSiblingThatZapped;
      }
      // Remove the pointer to procThatZappedMe
      process->procThatZappedMe = NULL;
  }
}

/*
 * Gets the current time in microseconds from USLOSS.
 */
int getCurrentTime()
{
    int time;
    int result = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &time);
    if (result == USLOSS_DEV_INVALID)
    {
        USLOSS_Console("getCurrentTime(): Bug in getCurrentTime code.\n");
        USLOSS_Halt(1);
    }
    return time;
}

/*
 * Prints the child list of the given parent. Used for debugging exclusively
 */
void printChildList(procPtr parent){
  USLOSS_Console("\tPrinting child list for parent %d:\n", parent->pid);
  if (parent->childProcPtr != NULL)
  {
      procPtr child = parent->childProcPtr;
      while (child != NULL)
      {
          USLOSS_Console("\t\t%d\n", child->pid);
          child = child->nextSiblingPtr;
      }
  }
}
