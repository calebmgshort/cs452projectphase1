/* ------------------------------------------------------------------------
   phase1utility.c
   Utility file for phase1.c

   University of Arizona
   Computer Science 452
   Fall 2015

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include "kernel.h"
#include "phase1utility.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Helper for fork1() that calculates the pid for the next process.
 *
 * TODO clear out dead processes when space is needed.
 */
static int getNextPid()
{
    int slot = pidToSlot(nextPid);
    procStruct proc = ProcTable[slot];

    if (proc.pid == 0)
    {
        return slot; // this slot was never occupied, so this pid is new
    }

    // slot is taken, use linear probing to search for an open slot
    for (int i = 0; i < MAXPROC; i++)
    {
        proc = ProcTable[(slot + i) % MAXPROC];
        if (proc.pid == 0)
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
static int pidToSlot(int pid)
{
    return (pid) % MAXPROC;
}
