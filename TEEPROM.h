#ifndef _TEEPROM_H_
#define _TEEPROM_H_

#include <xc.h>
#include <pic18f4321.h>
#include "Utils.h"

void EEPROM_Init(void);
// Post: Initializes EEPROM memory management and prepares for user configuration storage

BOOL EEPROM_StoreConfigForUser(BYTE user, const BYTE *led_config);
// Pre: user is valid user index, led_config is array of 6 bytes with values 0-10
// Post: Stores user's LED configuration in EEPROM (6 bytes: L0-L5) and returns TRUE when it's done.

BOOL EEPROM_ReadConfigForUser(BYTE user, BYTE *led_config);
// Pre: user is valid user index, led_config is array of at least 6 bytes
// Post: Reads user's LED configuration from EEPROM (6 bytes: L0-L5) and returns TRUE when it's done.

void EEPROM_CleanMemory(void);
// Post: Clears all stored user configurations and resets to default values
// All users will have default configuration (all LEDs off: 0,0,0,0,0,0)

#endif
