/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Fall 2017

   ------------------------------------------------------------------------ */
#include "phase1.h"
#include "kernel.h"
#include "phase1utility.h"
#include "queue.h"
#include "interrupt.h"

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

/* -------------------------- Globals ------------------------------------- */
// Patrick's debugging global variable...
int debugflag = 0;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
priorityQueue ReadyList;

// current process ID
procPtr Current = NULL;

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
    {
        USLOSS_Console("startup(): Initializing process table.\n");
    }
    for (int i = 0; i < MAXPROC; i++)
    {
        ProcTable[i].pid = 0;
    }

    // Initialize the Ready list
    if (DEBUG && debugflag)
    {
        USLOSS_Console("startup(): Initializing the ready list.\n");
    }
    initPriorityQueue(&ReadyList);

    // Initialize the clock interrupt handler
    if (DEBUG && debugflag)
    {
        USLOSS_Console("startup(): Initializing interrupt handlers.\n");
    }
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;

    // startup a sentinel process
    if (DEBUG && debugflag)
    {
        USLOSS_Console("startup(): Calling fork1() for sentinel.\n");
    }
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, SENTINELPRIORITY);
    if (result < 0)
    {
        USLOSS_Console("startup(): fork1 of sentinel returned error.  Halting...\n");
        USLOSS_Halt(1);
    }

    // start the test process
    if (DEBUG && debugflag)
    {
        USLOSS_Console("startup(): Calling fork1() for start1.\n");
    }
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0)
    {
        USLOSS_Console("startup(): fork1 for start1 returned an error.  ");
        USLOSS_Console("Halting...\n");
        USLOSS_Halt(1);
    }

    // We should never come back to this part of the code!
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
    {
        USLOSS_Console("finish(): Called.\n");
    }
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
    {
        USLOSS_Console("fork1(): Creating process %s.\n", name);
    }

    // ensure that we are in kernel mode
    checkMode("fork1");

    // enable interrupts
    enableInterrupts();

    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("fork1(): fork1 called with stack size < USLOSS_MIN_STACK.\n");
        }
        return -2;
    }

    // Get the next pid
    int pid = getNextPid();
    if (pid == -1)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("fork1(): No room in the process table.\n");
        }
        return -1;
    }
    nextPid = pid + 1;
    if (DEBUG && debugflag)
    {
        USLOSS_Console("fork1(): Pid of new process determined to be %d.\n", pid);
    }

    // Start to fill out entry in the process table.
    if (DEBUG && debugflag)
    {
        USLOSS_Console("fork1(): Initializing process table entry.\n");
    }
    procPtr proc = &ProcTable[pidToSlot(pid)];
    if (initProc(Current, proc, name, startFunc, arg, stacksize, priority, pid) == -1)
    {
        return -1;
    }

    // Add the new proc to Current's child list
    if (DEBUG && debugflag && Current != NULL)
    {
        USLOSS_Console("fork1(): Adding new process to child list of process %d.\n", Current->pid);
    }
    addChild(proc, Current);

    // for future phase(s)
    p1_fork(proc->pid);

    // Modify the ready list
    if (DEBUG && debugflag)
    {
        USLOSS_Console("fork1(): Adding process to the ready list.\n");
    }
    addProc(&ReadyList, proc);

    // Call the dispatcher
    if (priority != SENTINELPRIORITY)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("fork1(): Calling the dispatcher.\n");
        }
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
    {
        USLOSS_Console("launch(): Started.\n");
    }

    // Enable interrupts
    enableInterrupts();

    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
    {
        USLOSS_Console("Process %d returned to launch.\n", Current->pid);
    }

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
    // case 1: Has no quit or unquit children.
    if (Current->childProcPtr == NULL)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("join(): Process %d has no children to join.\n", Current->pid);
        }
        return -2;
    }

    // case 3: Only has children who have not yet quit.
    if (Current->quitChildPtr == NULL)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("join(): Process %d has no quit children. Blocking.\n", Current->pid);
        }

        // This process must block and wait
        Current->status = STATUS_BLOCKED_JOIN;
        // Switch to another process. When we switch back, we'll jump in after dispatcher().
        dispatcher();

        // Proceed like case 2 from here on
    }

    // Handle the case where the process was zapped while waiting for a child to quit
    if (Current->status == STATUS_ZAPPED)
    {
        return -1;
    }

    // case 2: At least 1 quit child waiting to be joined

    // Current->quitChildPtr should not be NULL at this point!
    if (Current->quitChildPtr == NULL)
    {
        USLOSS_Console("join(): Process %d has no quit children, when it absolutely must.\n", Current->pid);
        USLOSS_Halt(1);
    }

    // Remove the first quit child from the list.
    procPtr quitChild = Current->quitChildPtr;
    Current->quitChildPtr = Current->quitChildPtr->nextQuitSiblingPtr;

    // Extract info from quit child
    *status = quitChild->quitStatus;
    if (DEBUG && debugflag)
    {
        USLOSS_Console("join(): Process %d's child %d quit with status %d.\n", Current->pid, quitChild->pid, *status);
    }

    // Mark the quit child as dead
    quitChild->status = STATUS_DEAD;
    if (DEBUG && debugflag)
    {
        USLOSS_Console("join(): Removing quit child from child list.\n");
    }
    removeDeadChildren(Current);

    return quitChild->pid;
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
    procPtr childPtr = Current->childProcPtr;
    while(childPtr != NULL)
    {
        if (childPtr->status != STATUS_QUIT)
        {
            USLOSS_Console("quit(): process %d, '%s', has active children. Halting...\n", Current->pid, Current->name);
            USLOSS_Halt(1);
        }
        childPtr->status = STATUS_DEAD;
        childPtr = childPtr->nextSiblingPtr;
    }

    // Set current's status to quit
    if (DEBUG && debugflag)
    {
        USLOSS_Console("quit(): Process %d has called quit with status %d.\n", Current->pid, status);
    }
    Current->status = STATUS_QUIT;
    Current->quitStatus = status;

    // Notify parent that this process has quit
    procPtr parentPtr = Current->parentPtr;
    if (parentPtr != NULL)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("quit(): Notifying parent process %d.\n", parentPtr->pid);
        }
        addQuitChild(parentPtr, Current);

        // Set the parent's status to ready and add it to the process table
        if (parentPtr->status == STATUS_BLOCKED_JOIN)
        {
            if (DEBUG && debugflag)
            {
                USLOSS_Console("quit(): Parent was blocked on join. Unblocking.\n");
            }
            parentPtr->status = STATUS_READY;
            addProc(&ReadyList, parentPtr);
        }
    }

    // Unblock the processes that zapped this process
    unblockProcessesThatZappedThisProcess(Current);

    // For future phases
    p1_quit(Current->pid);

    // Call the dispatcher
    if (DEBUG && debugflag)
    {
        USLOSS_Console("quit(): Calling the dispatcher.\n");
    }
    dispatcher();
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
    // Add the runnning time (in microseconds) to the last process
    if (Current != NULL)
    {
        int deltaTime = getCurrentTime() - Current->startTime;
        if (DEBUG && debugflag)
        {
            USLOSS_Console("dispatcher(): Adding %d microseconds to running time for process %d.\n", deltaTime, Current->pid);
        }
        Current->CPUTime += deltaTime;
    }

    // Put the old process back on the ready list, if appropriate.
    if (Current != NULL && (Current->status == STATUS_READY || Current->status == STATUS_ZAPPED))
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("dispatcher(): The old process is still ready. Re-adding to ready list.\n");
        }
        addProc(&ReadyList, Current);
    }

    // Get the next process from the ready list
    procPtr nextProcess = removeProc(&ReadyList);
    if (nextProcess == NULL)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("dispatcher(): Ready list did not contain the sentinel!  Halting...\n");
        }
        USLOSS_Halt(1);
    }
    if (DEBUG && debugflag)
    {
        USLOSS_Console("dispatcher(): Next process is process %d.\n", nextProcess->pid);
    }

    // Switch Contexts
    USLOSS_Context *old = NULL;
    if (Current != NULL)
    {
        old = &(Current->state);
    }
    USLOSS_Context *new = &(nextProcess->state);
    if (Current != NULL)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("dispatcher(): Performing context switch from process %d to process %d\n",
                Current->pid, nextProcess->pid);
        }
        p1_switch(Current->pid, nextProcess->pid);
    }
    Current = nextProcess;
    // Update the running start time for the new Current
    Current->startTime = getCurrentTime();
    USLOSS_ContextSwitch(old, new);
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
    {
        USLOSS_Console("sentinel(): called\n");
    }
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */

/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
    for (int slot = 0; slot < MAXPROC; slot++)
    {
        // These are always sentinel and start1
        if (slot == 1 || slot == 2)
        {
            continue;
        }
        procPtr proc = &ProcTable[slot];
        if (processExists(proc))
        {
            // The sentinel is called even though other procs exist.
            // There must be a deadlock
            if (DEBUG && debugflag)
            {
                USLOSS_Console("checkDeadlock(): Found processes that are not sentinel or start1.  Halting...\n");
            }
            USLOSS_Halt(1);
        }
    }
    // Currently, there is no reason not to halt here
    USLOSS_Console("All processes completed.\n");
    USLOSS_Halt(0);
} /* checkDeadlock */
