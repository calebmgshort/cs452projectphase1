#include "phase1.h"

/*
 * Interrupt handler for the clock device.
 */
void clockHandler(int interruptType, void *arg)
{
    timeSlice();
}

void illegalIntHandler(int interruptType, void *arg)
{
  // Just a stub to statisfy test08 and test09
}
