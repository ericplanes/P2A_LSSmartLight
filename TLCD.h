#ifndef TLCD_H
#define TLCD_H

#include "Utils.h"
#include <xc.h>
#include <pic18f4321.h>

/* =======================================
 *        TLCD MODULE SPECIFICATIONS
 * ======================================= */
/*
 * HARDWARE REQUIREMENTS:
 * - HD44780-compatible LCD display (2x16 characters)
 * - 4-bit data interface
 * - Pin assignments:
 *   * RS (Register Select) -> RB4
 *   * RW (Read/Write)      -> RC5
 *   * E  (Enable)          -> RD7
 *   * D4-D7 (Data lines)   -> RB0-RB3
 *
 * DISPLAY FORMAT:
 * Line 1: "[C] HH:MM 1-X 2-Y"  (16 characters)
 * Line 2: "3-Z 4-W 5-V 6-U"    (15 characters)
 * Where:
 *   - C: Last character of user UID (or '-' for no user)
 *   - HH:MM: Current system time (maintained across all display states)
 *   - X,Y,Z,W,V,U: Light intensity values (0-9,A)
 *
 * DEPENDENCIES:
 * - TTimer module (uses TI_LCD timer)
 * - Utils.h (for BYTE, BOOL types)
 *
 * ROBUST INITIALIZATION:
 * - Double execution of complete initialization sequence
 * - 150ms power stabilization wait (75ms x 2 executions)
 * - Fixed timing delays during startup (NO busy flag checking)
 * - Busy flag usage only after LCD is fully configured
 * - Compatible with slow and non-compliant LCD modules
 */

void LCD_Init(void);
// Pre: TTimer module initialized
// Post: LCD ready for use (takes ~300ms)

void LCD_WriteNoUserInfo(void);
// Post: Display shows "no user" state with current system time

void LCD_WriteUserInfo(BYTE last_uid_char, BYTE hour, BYTE minute, BYTE *light_config);
// Pre: Valid printable char, hour [0-23], minute [0-59], light_config[6] with values [0x0-0xA]
// Post: Display shows complete user information and updates system time

void LCD_UpdateTime(BYTE hour, BYTE minute);
// Pre: hour [0-23], minute [0-59]
// Post: System time updated and displayed, preserves user char and light config

void LCD_UpdateLightConfig(BYTE *light_config);
// Pre: light_config[6] with values [0x0-0xA]
// Post: Light values updated, preserves user char and time

#endif
