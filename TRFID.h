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
// End pin definitions
//-------------------------------------------------

// Commands for PCD (Proximity Coupling Device) - Only used ones
#define PCD_IDLE 0x00       // NO action; Cancel the current command
#define PCD_AUTHENT 0x0E    // Authentication Key
#define PCD_TRANSCEIVE 0x0C // Transmit and receive data
#define PCD_RESETPHASE 0x0F // Reset
#define PCD_CALCCRC 0x03    // CRC Calculate

// Commands for PICC (Proximity Integrated Circuit Card) - Only used ones
#define PICC_REQIDL 0x26   // find the antenna area does not enter hibernation
#define PICC_ANTICOLL 0x93 // anti-collision
#define PICC_HALT 0x50     // Sleep

// Status codes
#define MI_OK 0
#define MI_NOTAGERR 1
#define MI_ERR 2

//------------------MFRC522 Registers (Only used ones)---------------
// Page 0: Command and Status
#define COMMANDREG 0x01
#define COMMIENREG 0x02
#define COMMIRQREG 0x04
#define DIVIRQREG 0x05
#define ERRORREG 0x06
#define STATUS2REG 0x08
#define FIFODATAREG 0x09
#define FIFOLEVELREG 0x0A
#define CONTROLREG 0x0C
#define BITFRAMINGREG 0x0D

// PAGE 1: Command
#define MODEREG 0x11
#define TXCONTROLREG 0x14
#define TXAUTOREG 0x15

// PAGE 2: CFG
#define CRCRESULTREGM 0x21
#define CRCRESULTREGL 0x22
#define TMODEREG 0x2A
#define TPRESCALERREG 0x2B
#define TRELOADREGH 0x2C
#define TRELOADREGL 0x2D

// UID related constants
#define RFID_UID_SIZE 5      // Size of UID in bytes
#define RFID_UID_STR_SIZE 15 // Size of UID string (5 bytes * 2 chars + 4 dashes + null terminator)

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
