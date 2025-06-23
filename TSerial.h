#ifndef _TSERIAL_H_
#define _TSERIAL_H_

#include <xc.h>
#include <pic18f4321.h>
#include "Utils.h"

/* =======================================
 *           TSERIAL MODULE
 * ======================================= */
/*
 * HARDWARE CONFIGURATION:
 * - UART communication at 9600 baud
 * - Pin assignments:
 *   * TX: RC6 - Transmit data to PC
 *   * RX: RC7 - Receive data from PC
 *
 * COMMUNICATION PROTOCOL:
 * - Commands: 1,2,3,ESC from PC keyboard
 * - Time input: HH:MM format
 * - Various formatted output messages to PC
 *
 * DEPENDENCIES:
 * - PIC18F4321 UART hardware module
 * - Utils.h for data types
 */

/* =======================================
 *              CONSTANTS
 * ======================================= */

// PC → PIC commands
#define CMD_NO_COMMAND 0
#define CMD_WHO_IN_ROOM 1
#define CMD_SHOW_STORED_CONF 2
#define CMD_UPDATE_TIME 3
#define CMD_ESC 4

// ASCII character defines
#define ASCII_1 '1'
#define ASCII_2 '2'
#define ASCII_3 '3'
#define ASCII_ESC 27

/* =======================================
 *         PUBLIC FUNCTION HEADERS
 * ======================================= */

void SIO_Init(void);
// Post: Initializes serial communication hardware

// Basic communication
BYTE SIO_ReadCommand(void);
// Pre: Serial hardware is initialized
// Post: Returns received command defined in the dictionary from serial buffer

BOOL SIO_ReadTime(BYTE *hour, BYTE *mins);
// Pre: Serial hardware is initialized, hour and mins point to valid BYTE variables
// Post: Returns FALSE until both hour and mins are filled; reads HH:MM format from serial

// Specific message functions
void SIO_SendDetectedCard(const BYTE *uid_bytes, const BYTE *config);
// Pre: uid_bytes points to 5-byte UID array, config points to 6-byte light configuration
// Post: Sends formatted card detection message to PC

void SIO_SendMainMenu(void);
// Post: Sends main menu options to PC

void SIO_SendUser(const BYTE *uid_bytes);
// Pre: uid_bytes points to 5-byte UID array
// Post: Sends user presence message to PC

void SIO_SendNoUser(void);
// Post: Sends no user present message to PC

void SIO_SendStoredConfig(const BYTE *uid_bytes, const BYTE *config);
// Pre: uid_bytes points to 5-byte UID array, config points to 6-byte light configuration
// Post: Sends stored configuration message to PC

void SIO_SendTimePrompt(void);
// Post: Sends time update prompt to PC

void SIO_SendUnknownCard(const BYTE *uid_bytes);
// Pre: uid_bytes points to 5-byte UID array
// Post: Sends unknown card message to PC

void SIO_SendKeyReset(void);
// Post: Sends keypad reset message to PC

#endif
