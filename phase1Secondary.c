#include "phase1.h"
#include "kernel.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "phase1utility.h"

extern procPtr Current;
extern procStruct ProcTable[];
extern priorityQueue ReadyList;

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
    if(Current->status == STATUS_ZAPPED)
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
    if(Current->status == STATUS_ZAPPED)
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
    if(process.status == STATUS_QUIT || process.status == STATUS_DEAD){
      continue;
    }
    short parentPid = -2;
    if(process.parentPtr != NULL)
    {
      parentPid = process.parentPtr->pid;
    }
    char status[30];
    switch(process.status)
    {
      case(STATUS_BLOCKED_ZAP):
        strcpy(status, "BLOCKED_ZAP");
        break;
      case(STATUS_BLOCKED_JOIN):
        strcpy(status, "BLOCKED_JOIN");
        break;
      case(STATUS_READY):
        strcpy(status, "READY");
        break;
      case(STATUS_ZAPPED):
        strcpy(status, "ZAPPED");
        break;
    }
    if(process.pid == Current->pid)
    {
      strcpy(status, "RUNNING");
    }
    int CPUTime = process.CPUTime;
    if(CPUTime == 0)
    {
        CPUTime = -1;
    }
    USLOSS_Console("%d\t  %d\t   %d\t\t%s\t\t  %d\t   %d\t%s\n",
        process.pid, parentPid, process.priority, status, numChildren(&process), CPUTime, process.name);
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

    // Change the status of the process being zapped
    processBeingZapped->status = STATUS_ZAPPED;

    // Add the current process to the list of processes that zapped the given process
    addZappedProcess(Current, processBeingZapped);

    // Change the status of the process that is zapping
    Current->status = STATUS_BLOCKED_ZAP;

    while(processBeingZapped->status != STATUS_QUIT)
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
