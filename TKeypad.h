#ifndef TKEYPAD_H
#define TKEYPAD_H

#include "Utils.h"
#include <xc.h>
#include <pic18f4321.h>

// Command types returned by KEY_GetCommand()
#define NO_COMMAND 0
#define UPDATE_LED 1
#define KEYPAD_RESET 2

void KEY_Init(void);
// Post: Initializes keypad hardware and internal state machine

void KEY_Reset(void);
// Post: Resets keypad state machine to initial conditions without reinitializing hardware

void KEY_Motor(void);
// Post: Processes keypad scanning, debouncing, and command detection

BYTE KEY_GetCommand(void);
// Post: Returns current command state: NO_COMMAND, UPDATE_LED, or RESET
// Resets internal command state to NO_COMMAND after returning a command

void KEY_GetUpdateInfo(BYTE *led, BYTE *intensity);
// Pre: KEY_GetCommand() returned UPDATE_LED
// Post: Fills led with LED number (0-5) and intensity with intensity value (0-10, where 10='*')
// Only valid immediately after KEY_GetCommand() returns UPDATE_LED

#endif
