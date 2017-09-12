#include "phase1.h"

/*
 * Interrupt handler for the clock device.
 */
void clockHandler(int interruptType, void *arg)
{
    timeSlice();
}
