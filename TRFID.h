#ifndef _TRFID_H_
#define _TRFID_H_

#include <xc.h>
#include <pic18f4321.h>
#include "Utils.h"

/* RFID-RC522 Pin Configuration
 * New pin assignments:
 * SDA (CS): RC0 - Chip Select (Slave Select)
 * SCK:      RC1 - Serial Clock
 * MOSI:     RC2 - Master Output Slave Input
 * MISO:     RC3 - Master Input Slave Output
 * IRQ:      Not connected
 * GND:      Ground
 * RST:      RD0 - Reset
 * 3V3:      3V3 power supply
 */

//------------------------------------------------
// RFID SPI Pin Definitions (Updated Configuration)
//-------------------------------------------------
#define MFRC522_SO PORTCbits.RC3   // input  (Master Input from Slave Output - MISO)
#define MFRC522_SI LATCbits.LATC2  // output (Master Output to Slave Input - MOSI)
#define MFRC522_SCK LATCbits.LATC1 // output (Serial Clock)
#define MFRC522_CS LATCbits.LATC0  // output (Chip Select - SDA pin on RC522 module)
#define MFRC522_RST LATDbits.LATD0 // output (Reset)

// Pin Direction Configuration
#define DIR_MFRC522_SO TRISCbits.TRISC3  // input  (MISO)
#define DIR_MFRC522_SI TRISCbits.TRISC2  // output (MOSI)
#define DIR_MFRC522_SCK TRISCbits.TRISC1 // output (Serial Clock)
#define DIR_MFRC522_CS TRISCbits.TRISC0  // output (Chip Select)
#define DIR_MFRC522_RST TRISDbits.TRISD0 // output (Reset)

//------------------------------------------------
// MFRC522 Commands (only used ones)
//-------------------------------------------------
#define PCD_IDLE 0x00       // NO action; Cancel the current command
#define PCD_TRANSCEIVE 0x0C // Transmit and receive data
#define PCD_RESETPHASE 0x0F // Reset
#define PCD_CALCCRC 0x03    // CRC Calculate

#define PICC_REQIDL 0x26   // find the antenna area does not enter hibernation
#define PICC_ANTICOLL 0x93 // anti-collision
#define PICC_HALT 0x50     // Sleep

#define MI_OK 0
#define MI_NOTAGERR 1
#define MI_ERR 2

//------------------MFRC522 Registers (only used ones)---------------
#define COMMANDREG 0x01
#define COMMIENREG 0x02
#define COMMIRQREG 0x04
#define ERRORREG 0x06
#define FIFODATAREG 0x09
#define FIFOLEVELREG 0x0A
#define BITFRAMINGREG 0x0D

#define RFID_UID_LENGTH 5
#define RFID_UID_STRING_LENGTH 15

//-------------- Public interface: --------------
void RFID_Init(void);
// Post: Initializes the RFID system

void RFID_Motor(void);
// Post: Cooperative motor that manages RFID card detection and reading

BOOL RFID_HasReadUser(void);
// Post: Returns TRUE if a user has been read, FALSE otherwise

BOOL RFID_GetReadUserId(BYTE *rfid_uid);
// Pre: RFID_HasReadUser() must return TRUE
// Post: Fills the rfid_uid position by position while returning FALSE. Returns TRUE once done

#endif
