#ifndef _TSERIAL_H_
#define _TSERIAL_H_

#include <xc.h>
#include <pic18f4321.h>
#include "Utils.h"

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
BOOL SIO_SendChar(BYTE character);
// Pre: Serial hardware is initialized
// Post: Returns TRUE if character sent successfully, FALSE otherwise

BYTE SIO_ReadCommand(void);
// Pre: Serial hardware is initialized
// Post: Returns received command defined in the dictionary from serial buffer

// Specific message functions
void SIO_SendDetectedCard(BYTE *UID, BYTE *config);
// Pre: UID points to 5-byte UID array, config points to 6-byte light configuration
// Post: Sends formatted card detection message to PC

void SIO_SendMainMenu(void);
// Post: Sends main menu options to PC

void SIO_SendUser(BYTE *User);
// Pre: User points to 5-byte UID array
// Post: Sends user presence message to PC

void SIO_SendNoUser(void);
// Post: Sends no user present message to PC

void SIO_SendStoredConfig(BYTE *UID, BYTE *config);
// Pre: UID points to 5-byte UID array, config points to 6-byte light configuration
// Post: Sends stored configuration message to PC

void SIO_SendTimePrompt(void);
// Post: Sends time update prompt to PC

void SIO_SendUnknownCard(BYTE *UID);
// Pre: UID points to 5-byte UID array
// Post: Sends unknown card message to PC

// For testing purposes
void SIO_TEST_SendString(BYTE *string);

#endif
