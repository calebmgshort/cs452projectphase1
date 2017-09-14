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
static int containsPID(pqPtr, int);
extern int debugflag;


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
    if(containsPID(pq, proc->pid))
    {
        if(DEBUG && debugflag)
        {
            USLOSS_Console("addProc(): Not adding %d to wait list because it exists already", proc->pid);
        }
        return;
    }
    queuePtr q = &(pq->queues[proc->priority - 1]);
    addProcFIFO(q, proc);
}

/*
 * Determines if the given priority queue contains the given pid. If yes, return 1. If not, return 0
 */
int containsPID(pqPtr pq, int pid)
{
    if(pq == NULL)
    {
        return 0;
    }
    for (int i = 0; i < SENTINELPRIORITY; i++)
    {
        queue singleQueue = pq->queues[i];
        procPtr node = singleQueue.head;
        while(node != NULL)
        {
            if(node->pid == pid)
                return 1;
            node = node->nextProcPtr;
        }
    }
    return 0;
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

void printPriorityQueue(pqPtr pq)
{
    USLOSS_Console("printPriorityQueue(): Now printing\n");
    if(pq == NULL)
    {
        USLOSS_Console("printPriorityQueue(): Priority queue is NULL.\n");
        return;
    }
    for (int i = 0; i < SENTINELPRIORITY; i++)
    {
        USLOSS_Console("Priority %d: ", i+1);
        queue singleQueue = pq->queues[i];
        procPtr node = singleQueue.head;
        while(node != NULL)
        {
          USLOSS_Console("%d, ", node->pid);
          node = node->nextProcPtr;
        }
        USLOSS_Console("\n");
    }
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
