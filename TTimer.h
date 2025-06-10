#ifndef TTIMER_H
#define TTIMER_H

#include "Utils.h"

#include <xc.h>
#include "pic18f4321.h"

#define TWO_MS 1       // 2ms per tick
#define ONE_SECOND 500 // 1 interruption every 2ms
#define ONE_MINUTE 60 * ONE_SECOND

#define TI_RFID 0
#define TI_KEYPAD 1
#define TI_LIGHTS 2
#define TI_LCD 3
#define TI_SERIAL 4

void Timer0_ISR(void);

void TiInit(void);
// Post: Constructor. It is a global precondition to have called this function before calling any other function of the TAD.

BYTE TiNewTimer(BYTE *TimerHandle);
// Post: Returns TRUE if a new timer was successfully created, and FALSE otherwise.
// Sets *TimerHandle to the assigned timer identifier, which is needed to use TI_GetTics and TI_ResetTics.

void TiResetTics(BYTE TimerHandle);
// Pre: Handle has been returned by TiNewTimer.
// Post: Starts the timing associated with 'TimerHandle', storing the time reference at the moment of the call.

WORD TiGetTics(BYTE TimerHandle);
// Pre: Handle has been returned by TiNewTimer.
// Post: Returns the number of ticks elapsed since the call to TI_ResetTics for the same TimerHandle.

#endif
