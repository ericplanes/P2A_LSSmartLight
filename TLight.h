#ifndef TLIGHT_H
#define TLIGHT_H

#include "Utils.h"
#include <xc.h>
#include <pic18f4321.h>

/* =======================================
 *           TLIGHT MODULE
 * ======================================= */
/*
 * HARDWARE CONFIGURATION:
 * - 6 LEDs controlled via PWM at 50Hz
 * - Pin assignments:
 *   * LED0 -> RD1
 *   * LED1 -> RD2
 *   * LED2 -> RD3
 *   * LED3 -> RC4
 *   * LED4 -> RC5
 *   * LED5 -> RD4
 *
 * INTENSITY LEVELS:
 * - 0: LED OFF (0% brightness)
 * - 1-9: Variable brightness (10%-90%)
 * - 10: LED fully ON (100% brightness)
 *
 * DEPENDENCIES:
 * - TTimer module (uses TI_LIGHTS timer)
 */

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
