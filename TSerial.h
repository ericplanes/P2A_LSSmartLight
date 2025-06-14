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

// Basic communication
BOOL SIO_SendChar(BYTE character);
void SIO_TEST_SendString(BYTE *string);
BYTE SIO_ReadChar(void);

// Specific message functions
void SIO_SendDetectedCard(BYTE *UID, BYTE *config);
void SIO_SendMainMenu(void);
void SIO_SendUser(BYTE *User);
void SIO_SendNoUser(void);
void SIO_SendStoredConfig(BYTE *UID, BYTE *config);
void SIO_SendTimePrompt(void);
void SIO_SendUnknownCard(BYTE *UID);

#endif
