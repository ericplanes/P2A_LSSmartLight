#ifndef TLIGHT_H
#define TLIGHT_H

#include "Utils.h"
#include <xc.h>
#include <pic18f4321.h>

/* =======================================
 *         PUBLIC FUNCTION HEADERS
 * ======================================= */

void LED_Init(void);
// Post: Initializes LED ports as outputs and sets all LEDs to OFF (config = 0)

void LED_Motor(void);
// Post: Performs PWM control of all LEDs based on current configuration and timer tics

void LED_UpdateConfig(BYTE *config);
// Pre: config points to 6-byte array with LED intensities (0-10 for each LED)
// Post: Updates internal LED configuration array with new values

#endif
