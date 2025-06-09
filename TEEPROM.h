#ifndef _TEEPROM_H_
#define _TEEPROM_H_

#include <xc.h>
#include <pic18f4321.h>
#include "Utils.h"

void EEPROM_Init(void);
BOOL EEPROM_StoreUserConfig(BYTE uid_index, BYTE *light_config);
BOOL EEPROM_ReadUserConfig(BYTE uid_index, BYTE *light_config);
void EEPROM_CleanMemory(void);
BOOL EEPROM_IsConfigurationStored(BYTE uid_index);

#endif
