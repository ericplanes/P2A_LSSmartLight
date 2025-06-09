#ifndef TLCD_H
#define TLCD_H

#include "Utils.h"
#include <xc.h>
#include <pic18f4321.h>

void LCD_Init(void);
// Post: Initializes LCD in 4-bit mode and prepares for display operations

void LCD_Reset(void);
// Post: Shows initial state with no user present and all lights off
// Format: "- HH:MM 1-0 2-0 3-0 4-0 5-0 6-0"

void LCD_WriteUserInfo(BYTE last_uid_char, BYTE hour, BYTE minute, BYTE *light_config);
// Pre: light_config is an array of 6 bytes with light intensities (0x0-0xA)
// Post: Displays current user and light configuration
// Format: "F 16:30 1-0 2-3 3-3 4-0 5-9 6-A"

void LCD_UpdateTime(BYTE hour, BYTE minute);
// Post: Updates only the time portion of the display without changing other info

void LCD_UpdateLightConfig(BYTE *light_config);
// Pre: light_config is an array of 6 bytes with light intensities (0x0-0xA)
// Post: Updates only the light configuration without changing user char or time

#endif
