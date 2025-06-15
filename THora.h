#ifndef THORA_H
#define THORA_H

#include <xc.h>
#include <pic18f4321.h>
#include "Utils.h"

void HORA_Init(void);
// Post: Initializes hour and minutes to 0 and resets motor state

void HORA_Motor(void);
// Post: Increments time every minute using ONE_MINUTE timer
// Handles minute overflow (>59) by incrementing hour and resetting minutes

void HORA_GetTime(BYTE *hour, BYTE *minutes);
// Post: Fills hour and minutes pointers with current time values

void HORA_SetTime(BYTE hour, BYTE minutes);
// Pre: hour (0-99), minutes (0-59)
// Post: Sets current time to specified hour and minutes

#endif
