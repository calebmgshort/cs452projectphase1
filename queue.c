/* ------------------------------------------------------------------------
   queue.c
   Defines functions that manipulate a priority queue struct that allows us
   to store processes and access the highest priority process in the queue.

   University of Arizona
   Computer Science 452
   Fall 2015
   ------------------------------------------------------------------------ */
#include "queue.h"
#include <stdbool.h>
#include <stdlib.h>

/* ------------------------- Prototypes ----------------------------------- */
static bool isEmpty(queuePtr);
static void addProcFIFO(queuePtr, procPtr);
static procPtr removeProcFIFO(queuePtr);


/* -------------------------- Functions ----------------------------------- */
/*
 * Initializes a priority queue. Must be called on any priority queue before it
 * can be used.
 */
void initPriorityQueue(pqPtr pq)
{
    for (int i = 0; i < SENTINELPRIORITY; i++)
    {
        pq->queues[i].head = NULL;
        pq->queues[i].tail = NULL;
    }
}

/*
 * Adds the process proc to the back of the priority queue pq. pq must be non-
 * NULL. proc is assumed to have a NULL nextProcPtr.
 */
void addProc(pqPtr pq, procPtr proc)
{
    queuePtr q = &(pq->queues[proc->priority - 1]);
    addProcFIFO(q, proc);
}

/*
 * Removes the highest priority process in pq. If multiple processes have the
 * same priority, the first processes added to pq will be removed. Returns the
 * removed process. pq can never be empty (it must always contain the sentinel)
 * or NULL.
 */
procPtr removeProc(pqPtr pq)
{
    queuePtr q = NULL;

    for (int i = 0; i < SENTINELPRIORITY; i++)
    {
        q = &(pq->queues[i]);
        if (!isEmpty(q))
        {
            break;
        }
    }
    return removeProcFIFO(q);
}

/*
 * Returns true iff q is empty. q must be non-NULL.
 */
static bool isEmpty(queuePtr q)
{
    return q->head == NULL;
}

/*
 * Adds the process proc to the back of thie FIFO queue q. q must be non-NULL.
 * proc is assumed to have a NULL nextProcPtr and cannot be NULL.
 */
static void addProcFIFO(queuePtr q, procPtr proc)
{
    if (isEmpty(q))
    {
        q->head = proc;
        q->tail = proc;
    }
    else
    {
        q->tail->nextProcPtr = proc;
        q->tail = proc;
    }
}

/*
 * Removes the oldest process in the FIFO queue q and returns it. Returns NULL
 * iff q is empty. q must be non-NULL.
 */
static procPtr removeProcFIFO(queuePtr q)
{
    if (isEmpty(q))
    {
        return NULL;
    }
    procPtr ret = q->head;
    q->head = ret->nextProcPtr;
    ret->nextProcPtr = NULL;
    return ret;
}

