#ifndef THORA_H
#define THORA_H

#include <xc.h>
#include <pic18f4321.h>
#include "Utils.h"

/* =======================================
 *           THORA MODULE
 * ======================================= */
/*
 * TIME MANAGEMENT SYSTEM
 * - Maintains system time in HH:MM format
 * - Updates automatically based on timer interrupts
 * - Provides time display for LCD interface
 *
 * DEPENDENCIES:
 * - TTimer module for time base generation
 */

/* =======================================
 *         PUBLIC FUNCTION HEADERS
 * ======================================= */

void HORA_Init(void);
// Post: Initializes time management system
// Sets initial time to 00:00
// Starts automatic time updating

void HORA_Motor(void);
// Post: Updates system time based on timer ticks
// Increments minutes/hours as needed
// Manages 24-hour time format (00:00 - 23:59)

void HORA_GetTime(BYTE *hour, BYTE *minutes);
// Post: Returns current time via output parameters
// *hour contains current hour (0-23)
// *minutes contains current minute (0-59)

void HORA_SetTime(BYTE hour, BYTE minutes);
// Pre: hour (0-23), minutes (0-59)
// Post: Sets system time to specified values
// Updates internal time counters

#endif
