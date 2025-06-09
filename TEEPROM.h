#ifndef _TEEPROM_H_
#define _TEEPROM_H_

#include <xc.h>
#include <pic18f4321.h>
#include "Utils.h"

void EEPROM_Init(void);
// Post: Initializes EEPROM memory management and prepares for user configuration storage

BOOL EEPROM_StoreUserConfig(BYTE uid_index, BYTE *light_config);
// Pre: uid_index is valid user index, light_config is array of 6 bytes with values 0x0-0xA
// Post: Stores user's light configuration in EEPROM with validity flag
// Returns: TRUE if storage successful, FALSE if error occurred

BOOL EEPROM_ReadUserConfig(BYTE uid_index, BYTE *light_config);
// Pre: uid_index is valid user index, light_config is array of at least 6 bytes
// Post: Reads user's light configuration from EEPROM if valid configuration exists
// Returns: TRUE if valid configuration found and read, FALSE if no valid configuration

void EEPROM_CleanMemory(void);
// Post: Clears all stored user configurations and resets validity flags
// All users will have default configuration (all lights off)

BOOL EEPROM_IsConfigurationStored(BYTE uid_index);
// Pre: uid_index is valid user index
// Post: Checks if user has a valid configuration stored in EEPROM
// Returns: TRUE if valid configuration exists, FALSE otherwise

#endif
