/* Patrick's DEBUG printing constant... */
#ifndef _KERNEL_H
#define _KERNEL_H

#include "phase1.h"

#define DEBUG 1

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
   procPtr         nextProcPtr;
   procPtr         childProcPtr;
   procPtr         nextSiblingPtr;
   procPtr         parentPtr;
   procPtr         quitChildPtr;
   procPtr         nextQuitSiblingPtr;
   procPtr         procThatZappedMe;
   procPtr         nextSiblingThatZapped;
   char            name[MAXNAME];     /* process's name */
   char            startArg[MAXARG];  /* args passed to process */
   USLOSS_Context  state;             /* current context for process */
   short           pid;               /* process id */
   int             priority;
   int (* startFunc) (char *);   /* function where process begins -- launch */
   char           *stack;
   unsigned int    stackSize;
   int             status;        /* READY, BLOCKED, QUIT, etc. */
   int             quitStatus;
   /* other fields as needed... */
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
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
