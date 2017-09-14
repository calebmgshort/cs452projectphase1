#include "phase1.h"
#include "kernel.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "phase1utility.h"

extern procPtr Current;
extern procStruct ProcTable[];
extern priorityQueue ReadyList;
extern int debugflag;

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
    if(block_status <= 10)
    {
        USLOSS_Console("Error. blockMe received an order to block a process with status < 10");
        USLOSS_Halt(1);
    }
    if(Current->isZapped)
        return -1;
    Current->status = block_status;
    return 0;
}

/*
 * This operation unblocks process pid that had previously blocked itself by calling blockMe.
 * The status of that process is changed to READY, and it is put on the Ready List. The dispatcher
 * will be called as a side-effect of this function.
 * Return values:
 * -2: if the indicated process was not blocked, does not exist, is the current process, or is
 * blocked on a status less than or equal to 10. Thus, a process that is zap-blocked or
 * join-blocked cannot be unblocked with this function call.
 * -1: if the calling process was zapped. 0: otherwise.
 */
int unblockProc(int pid)
{
    //  Make sure everything is valid
    procPtr process = &ProcTable[pidToSlot(pid)];
    if(!processExists(process) || process->pid == Current->pid || process->status <= 10)
        return -2;
    if(Current->isZapped)
        return -1;
    // Set the process's status to ready
    process->status = STATUS_READY;
    // Add the process to the readly list
    addProc(&ReadyList, process);
    // Call the dispatcher
    dispatcher();
    return 0;
}

/*
 * This operation returns the time (in microseconds) at which the currently executing process
 * began its current time slice.
 */
int readCurStartTime(void)
{
    return Current->startTime;
}

/*
 * This operation calls the dispatcher if the currently executing process has exceeded its time slice;
 * otherwise, it simply returns.
 */
 void timeSlice()
 {
     int currentTime = getCurrentTime();

     if (currentTime - Current->startTime > MAX_TIME_SLICE)
     {
         dispatcher();
     }
     return;
 }

/*
 * Return the CPU time (in milliseconds) used by the current process.
 */
int readtime(void)
{
    return Current->CPUTime/1000;
}

/*
 * This routine prints process information to the console. For each PCB in the process table,
 * print its PID, parent’s PID, priority, process status (e.g. empty, running, ready, blocked,
 * etc.), number of children, CPU time consumed, and name.
 */
void dumpProcesses()
{
  USLOSS_Console("PID\tParent\tPriority\tStatus\t\t#Kids\tCPUtime\tName\n");
  int i;
  for(i = 0; i < 50; i++)
  {
    procStruct process = ProcTable[i];

    USLOSS_Console("%d\t  ", process.pid);

    short parentPid = PID_NEVER_EXISTED;
    if(process.parentPtr != NULL)
    {
      parentPid = process.parentPtr->pid;
    }

    USLOSS_Console("%d\t   ", parentPid);

    USLOSS_Console("%d\t\t", process.priority);

    char status[30];
    if(Current != NULL && process.pid == Current->pid)
    {
      strcpy(status, "RUNNING\t");
    }
    else
    {
      switch(process.status)
      {
        case(STATUS_EMPTY):
          strcpy(status, "EMPTY\t");
          break;
        case(STATUS_BLOCKED_ZAP):
          strcpy(status, "ZAP_BLOCK");
          break;
        case(STATUS_BLOCKED_JOIN):
          strcpy(status, "JOIN_BLOCK");
          break;
        case(STATUS_READY):
          if (process.isZapped)
          {
            strcpy(status, "ZAPPED\t");
          }
          else
          {
            strcpy(status, "READY\t");
          }
          break;
        case(STATUS_QUIT):
          strcpy(status, "QUIT\t");
          break;
        case(STATUS_DEAD):
          strcpy(status, "DEAD\t");
          break;
        default:
          strcpy(status, "UNKOWN\t");
          break;
      }
    }
    USLOSS_Console("%s\t  ", status);

    USLOSS_Console("%d\t   ", numChildren(&process));

    int CPUTime = process.CPUTime;
    if(CPUTime == 0)
    {
        CPUTime = -1;
    }

    USLOSS_Console("%d\t%s\n", CPUTime, process.name);
    //if(numChildren(&process) == 1){
    //  USLOSS_Console("%d\n", process.childProcPtr->pid);
    //  USLOSS_Console("%d\n", process.childProcPtr->nextProcPtr != NULL);
    //}
  }
}

/*
 * Returns the pid of the calling process
 */
int getpid()
{
    return Current->pid;
}


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
    // ensure that we are in kernel mode
    checkMode("zap");
    disableInterrupts();

    if (DEBUG && debugflag)
    {
        USLOSS_Console("zap(): Process %d now zapping process %d\n", Current->pid, pid);
    }
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
        USLOSS_Console("zap(): process %d tried to zap itself.  Halting...\n", pid);
        USLOSS_Halt(1);
    }
    else if(processBeingZapped->status == STATUS_QUIT)
    {
        USLOSS_Console("zap failed because the given process has already quit\n");
        USLOSS_Halt(1);
    }

    // Change the status of the process being zapped
    processBeingZapped->isZapped = 1;

    if (DEBUG && debugflag)
    {
        USLOSS_Console("zap(): Adding the zapper to list of processes that zapped the zappee\n");
    }
    // Add the current process to the list of processes that zapped the given process
    addZappedProcess(Current, processBeingZapped);

    // Change the status of the process that is zapping
    Current->status = STATUS_BLOCKED_ZAP;

    if (DEBUG && debugflag)
    {
        USLOSS_Console("zap(): Now waiting for the zappee to quit\n");
    }

    while(processBeingZapped->status != STATUS_QUIT && processBeingZapped->status != STATUS_DEAD)
    {
        if(Current->isZapped)
        {
            if (DEBUG && debugflag)
            {
                USLOSS_Console("zap(): The zapper %d was zapped while waiting for the zappee %d to quit\n", Current->pid, pid);
            }
            return -1;
        }
        if (DEBUG && debugflag)
        {
            USLOSS_Console("zap(): Process %d is waiting for zapped process %d to quit\n", Current->pid, pid);
        }
        // Call the sentinel since this process can't do anything right now anyway
        dispatcher();
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
    if(Current->isZapped)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
