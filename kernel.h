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
    char           *stack;                   // call stack for this process
    unsigned int    stackSize;
    int             status;                  // the current status of this proc (blocked, ready, etc)
    int             quitStatus;              // the exit status of this proc, if it has already quit
    int             startTime;               // The time at which this process last started executing (microseconds).
    long            CPUTime;                 // The amount of time this process has run (microseconds)
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
#define MAX_TIME_SLICE 80000

// Status codes
#define STATUS_EMPTY -1        // This process has never been initialized
#define STATUS_READY 0         // Should be on the ready list.
#define STATUS_BLOCKED_ZAP 1   // Blocked because this is zapping another process.
#define STATUS_QUIT 2          // This process has quit.
#define STATUS_ZAPPED 3        // This process has been zapped. It should quit eventually.
#define STATUS_BLOCKED_JOIN 4  // Blocked waiting for a child to quit.
#define STATUS_DEAD 5          // This process has quit and has been joined by its parent.

#define PID_NEVER_EXISTED -1

#endif
