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

int getNextPid();
int pidToSlot(int);
bool processExists(procStruct);

#endif
