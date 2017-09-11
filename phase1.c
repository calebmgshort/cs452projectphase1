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
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static priorityQueue ReadyList;

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
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
    }
    for (int i = 0; i < MAXPROC; i++)
    {
        ProcTable[i].pid = 0;
    }

    // Initialize the Ready list
    if (DEBUG && debugflag)
    {
        USLOSS_Console("startup(): initializing the Ready list\n");
    }
    initPriorityQueue(&ReadyList);

    // Initialize the clock interrupt handler
    // USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;

    // startup a sentinel process
    if (DEBUG && debugflag)
    {
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    }
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
    {
        USLOSS_Console("startup(): calling fork1() for start1\n");
    }
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
        USLOSS_Console("in finish...\n");
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
        USLOSS_Console("fork1(): creating process %s\n", name);
    }

    // ensure that we are in kernel mode
    checkMode("fork1");

    // enable interrupts
    if (DEBUG && debugflag)
    {
        USLOSS_Console("fork1(): enabling interrupts.\n");
    }
    enableInterrupts();

    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK)
    {
        if (DEBUG && debugflag)
        {
            USLOSS_Console("fork1(): Fork called with stack size < USLOSS_MIN_STACK.\n");
        }
        return -2;
    }

    // Is there room in the process table? What is the next PID?
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

    // Initialize proc's fields.
    if (initProc(Current, proc, name, startFunc, arg, stacksize, priority, pid) == -1)
    {
        return -1;
    }

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
        USLOSS_Console("launch(): started\n");
    }

    // Enable interrupts
    enableInterrupts();

    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
    {
        USLOSS_Console("Process %d returned to launch\n", Current->pid);
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
    // case 2: At least 1 quit child waiting to be joined

    // Current->quitChildPtr should not be NULL at this point!
    if(Current->quitChildPtr == NULL)
    {
        USLOSS_Console("Error. Process %d has no quit children, when it absolutely must.\n", Current->pid);
        USLOSS_Halt(1);
    }

    // TODO: Handle the case where the process was zappd while waiting for a child to quit

    Current->quitChildPtr->status = STATUS_DEAD;
    *status = Current->quitChildPtr->quitStatus
    int quitPid = Current->quitChildPtr->pid;
    Current->quitChildPtr = Current->quitChildPtr->nextQuitSiblingPtr;
    if (DEBUG && debugflag)
    {
        USLOSS_Console("Process %d's child %d quit with status %d.\n", Current->pid, quitPid, *status);
    }
    return quitPid;
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
            USLOSS_Console("quit(): Process attempting to quit with living children.  Halting...\n");
            USLOSS_Halt(1);
        }
        childPtr = childPtr->nextSiblingPtr;
    }
    Current->status = STATUS_QUIT;
    Current->quitStatus = status;
    // Notify parent that this process has quit
    procPtr parentPtr = Current->parentPtr;
    if (parentPtr != NULL)
    {
        if(parentPtr->quitChildPtr == NULL)
        {
            parentPtr->quitChildPtr = Current;
        }
        else
        {
            procPtr quitChildPtr = parentPtr->quitChildPtr;
            // add Current to nextQuitSiblingPtr list
            while(quitChildPtr->nextQuitSiblingPtr != NULL)
            {
                quitChildPtr = quitChildPtr->nextQuitSiblingPtr;
            }
            quitChildPtr->nextQuitSiblingPtr = Current;
        }
        if(parentPtr->status == STATUS_BLOCKED_JOIN)
        {
            parentPtr->status = STATUS_READY;
            addProc(&ReadyList, parentPtr);
        }
    }

    // Unblock the processes that zapped this process
    if(Current->procThatZappedMe != NULL)
    {
        procPtr procThatZappedMe = Current->procThatZappedMe;
        // Set each of these processes to ready
        while(procThatZappedMe != NULL)
        {
            procThatZappedMe->status = STATUS_READY;
            addProc(&ReadyList, procThatZappedMe);
            procThatZappedMe = procThatZappedMe->nextSiblingThatZapped;
        }
        // Remove the pointer to nextSiblingThatZapped for each
        procThatZappedMe = Current->procThatZappedMe;
        procPtr last = procThatZappedMe;
        procThatZappedMe = procThatZappedMe->nextSiblingThatZapped;
        while(procThatZappedMe != NULL)
        {
            last->nextSiblingThatZapped = NULL;
            last = procThatZappedMe;
            procThatZappedMe = procThatZappedMe->nextSiblingThatZapped;
        }
        // Remove the pointer to procThatZappedMe
        Current->procThatZappedMe = NULL;
    }

    p1_quit(Current->pid);
    dispatcher();
} /* quit */

/* ------------------------------------------------------------------------
   Name - zap
   Purpose - Zaps a process with the given process id
   Parameters - the process id of the process to zap
   Returns - -1: the calling process itself was zapped while in zap.
              0: the zapped process has called quit.
   Side Effects - Forces another process to quit
   ------------------------------------------------------------------------ */
int zap(int pid)
{
    procPtr processBeingZapped = &ProcTable[pidToSlot(pid)];
    if(pid < 0)
    {
        USLOSS_Console("zap failed because the given pid was out of range of the process table\n");
        USLOSS_Halt(1);
    }
    else if(!processExists(processBeingZapped))
    {
        USLOSS_Console("zap failed because the given process does not exist\n");
        USLOSS_Halt(1);
    }
    else if(processBeingZapped == Current)
    {
        USLOSS_Console("zap failed because the given process to zap is itself\n");
        USLOSS_Halt(1);
    }
    else if(processBeingZapped->status == STATUS_QUIT)
    {
        USLOSS_Console("zap failed because the given process has already quit\n");
        USLOSS_Halt(1);
    }

    // Add the current process to the list of processes that zapped the given process
    processBeingZapped->status = STATUS_ZAPPED;
    if(processBeingZapped->procThatZappedMe != NULL)
    {
        processBeingZapped->procThatZappedMe = Current;
    }
    else
    {
        procPtr procThatZapped = processBeingZapped->procThatZappedMe;
        while(procThatZapped->nextSiblingThatZapped != NULL)
        {
            procThatZapped = procThatZapped->nextSiblingThatZapped;
        }
        procThatZapped->nextSiblingThatZapped = Current;
    }

    Current->status = STATUS_BLOCKED_ZAP;

    while(ProcTable[pidToSlot(pid)].status != STATUS_QUIT)
    {
        if(Current->status == STATUS_ZAPPED)
        {
            return -1;
        }
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
int isZapped(void)
{
    if(Current->status == STATUS_ZAPPED)
    {
        return 1;
    }
    else
    {
        return 0;
    }
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
    // Put the old process back on the ready list, if appropriate.
    if (Current != NULL && Current->status == STATUS_READY) // TODO any other statuses?
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
} /* checkDeadlock */

/*
 * Returns the pid of the calling process
 */
int getpid()
{
  return Current->pid;
}

/*
 * This routine prints process information to the console. For each PCB in the process table,
 * print its PID, parent’s PID, priority, process status (e.g. empty, running, ready, blocked,
 * etc.), number of children, CPU time consumed, and name.
 */
void dumpProcesses()
{
  USLOSS_Console("PID\tParent PID\tPriority\tProcess Status\tNumber of Children\tCPU Time Consumed\tName\n");
  int i;
  for(i = 0; i < 50; i++)
  {
    procStruct process = ProcTable[i];
    if(process.status == STATUS_QUIT){
      continue;
    }
    short parentPid = -1;
    if(process.parentPtr != NULL)
    {
      parentPid = process.parentPtr->pid;
    }
    char status[30];
    switch(process.status)
    {
      case(STATUS_BLOCKED_ZAP):
        strcpy(status, "STATUS_BLOCKED_ZAP");
        break;
      case(STATUS_BLOCKED_JOIN):
        strcpy(status, "STATUS_BLOCKED_JOIN");
        break;
      case(STATUS_READY):
        strcpy(status, "STATUS_READY");
        break;
      case(STATUS_ZAPPED):
        strcpy(status, "STATUS_ZAPPED");
        break;
    }
    // TODO: Test this to make it clean, also determine how to calculate CPU time
    USLOSS_Console("%d\t%d\t%d\t%s\t%d\t???\t%s\n",
        process.pid, parentPid, process.priority, status, numChildren(&process), process.name);
  }

  /*
   * This operation will block the calling process. newStatus is the value used to indicate the
   * status of the process in the dumpProcesses command. newStatus must be greater than 10; if
   * it is not, then halt USLOSS with an appropriate error message.
   * Return values:
   * -1: if process was zapped while blocked.
   *  0: otherwise.
   */
  int blockMe(int block_status)
  {
    // TODO: Write this function
  }

  /*
   * This operation returns the time (in microseconds) at which the currently executing process
   * began its current time slice.
   */
  int readCurStartTime(void)
  {
    // TODO: Write this function
  }

  /*
   * This operation calls the dispatcher if the currently executing process has exceeded its time slice;
   * otherwise, it simply returns.
   */
  void timeSlice(void)
  {
    int startTime = readCurStartTime();
    int curTime = USLOSS_DeviceInput();
  }
  
  /*
   * Return the CPU time (in milliseconds) used by the current process.
   */
  int readtime(void)
  {
    // TODO: Write this function
  }
}
