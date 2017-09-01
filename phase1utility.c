/* ------------------------------------------------------------------------
   phase1utility.c
   Utility file for phase1.c

   University of Arizona
   Computer Science 452
   Fall 2015

   ------------------------------------------------------------------------ */

#include "phase1utility.h"

extern unsigned int nextPid;
extern procStruct ProcTable[];

/*
 * Helper for fork1() that calculates the pid for the next process.
 *
 * TODO clear out dead processes when space is needed.
 */
int getNextPid()
{
    int slot = pidToSlot(nextPid);
    procStruct proc = ProcTable[slot];

    if (!processExists(proc))
    {
        return slot; // this slot was never occupied, so this pid is new
    }

    // slot is taken, use linear probing to search for an open slot
    for (int i = 0; i < MAXPROC; i++)
    {
        proc = ProcTable[(slot + i) % MAXPROC];
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

bool processExists(procStruct process){
  if(process.pid == 0)
    return false;
  else
    return true;
}
