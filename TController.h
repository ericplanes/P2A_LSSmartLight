#ifndef TCONTROLLER_H
#define TCONTROLLER_H

#include "Utils.h"
#include <xc.h>
#include <pic18f4321.h>

/* =======================================
 *        TCONTROLLER MODULE
 * ======================================= */
/*
 * MAIN SYSTEM CONTROLLER
 * - Manages overall system state and user flow
 * - Coordinates between all subsystems (RFID, Keypad, LCD, Serial, LEDs, EEPROM)
 * - Implements the complete smart lighting system logic
 *
 * DEPENDENCIES:
 * - All other TAD modules (TSerial, TLCD, TKeypad, TLight, TRFID, TEEPROM, TUserControl, THora)
 */

/* =======================================
 *         PUBLIC FUNCTION HEADERS
 * ======================================= */

void CNTR_Init(void);
// Post: Initializes the controller and performs startup protocol
// - Sends main menu via serial
// - Displays initial empty state on LCD
// - Initializes internal state variables

void CNTR_Motor(void);
// Post: Executes one cycle of the main system control loop
// - Manages user presence state
// - Processes keypad commands when user is inside
// - Handles RFID card detection and user authentication
// - Processes serial commands from PC
// - Coordinates all subsystem interactions

#endif
