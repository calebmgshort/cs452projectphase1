/*
 * These are the definitions for phase1utility, a utility class for phase1.c
 */

#ifndef _PHASE1UTILITY_H
#define _PHASE1UTILITY_H

#include "phase1.h"
#include "kernel.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <usloss.h>
#include <queue.h>

int getNextPid();
int pidToSlot(int);
bool processExists(procPtr);
bool inKernelMode();
int initProc(procPtr, procPtr, char *, int(*startFunc)(char *), char *, int, int, int);
void checkMode(char *);
int numChildren(procPtr);
void enableInterrupts();
void disableInterrupts();
void addChild(procPtr, procPtr);
void addQuitChild(procPtr, procPtr);
void removeDeadChildren(procPtr);
void addZappedProcess(procPtr, procPtr);
void unblockProcessesThatZappedThisProcess(procPtr);
int getCurrentTime();

// Functions used only for debugging
void printChildList(procPtr);

#endif
