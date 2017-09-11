#ifndef _KERNEL_H
#define _KERNEL_H

#include "phase1.h"

/* Patrick's DEBUG printing constant... */
#define DEBUG 1

typedef struct procStruct procStruct;
typedef struct procStruct * procPtr;

struct procStruct
{
    procPtr         nextProcPtr;             // Linked list ptr used by the ready list

    procPtr         childProcPtr;            // Linked list storing this proc's children
    procPtr         nextSiblingPtr;

    procPtr         quitChildPtr;            // Linked list storing this proc's quit children
    procPtr         nextQuitSiblingPtr;

    procPtr         procThatZappedMe;        // Linked list of procs that have zapped this proc
    procPtr         nextSiblingThatZapped;

    procPtr         parentPtr;               // The parent of this process
    char            name[MAXNAME];           // process's name
    char            startArg[MAXARG];        // args passed to process
    USLOSS_Context  state;                   // current context for process
    short           pid;                     // process id
    int             priority;                // process priority
    int (* startFunc) (char *);              // function where this process begins
    char           *stack;
    unsigned int    stackSize;
    int             status;                  // the current status of this proc (blocked, ready, etc)
    int             quitStatus;              // the exit status of this proc, if it has already quit
};

struct psrBits
{
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues
{
    struct psrBits bits;
    unsigned int integerPart;
};

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)

// Status codes
#define STATUS_READY 0         // Should be on the ready list.
#define STATUS_BLOCKED 1       // Blocked because this is zapping another process. TODO rename
#define STATUS_QUIT 2          // This process has quit.
#define STATUS_ZAPPED 3        // This process has been zapped. It should quit eventually.
#define STATUS_BLOCKED_JOIN 4  // Blocked waiting for a child to quit.

#endif
