/* ------------------------------------------------------------------------
   queue.h
   Header for queue.c. Contains typedefs for priority/FIFO procStruct queues.
   Include this file for access to the priority queue manipulation functions.

   University of Arizona
   Computer Science 452
   Fall 2015
   ------------------------------------------------------------------------ */
#ifndef _QUEUE_H
#define _QUEUE_H

#include "kernel.h"

typedef struct queue queue;
typedef struct queue * queuePtr;
typedef struct priorityQueue priorityQueue;
typedef struct priorityQueue * pqPtr;

struct queue
{
    procPtr head;
    procPtr tail;
};

struct priorityQueue
{
     queue queues[SENTINELPRIORITY]; 
};

void initPriorityQueue(pqPtr);
void addProc(pqPtr, procPtr);
procPtr removeProc(pqPtr);

#endif /* _QUEUE_H */

