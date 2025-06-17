#include <xc.h>
#include <pic18f4321.h>

#include "Utils.h"
#include "TTimer.h"
#include "TEEPROM.h"
#include "TLCD.h"
#include "TKeypad.h"
#include "THora.h"
#include "TSerial.h"
#include "TLight.h"
#include "TUserControl.h"
#include "TRFID.h"

// Configuration bits
#pragma config OSC = INTIO2
#pragma config PBADEN = DIG
#pragma config MCLRE = OFF
#pragma config DEBUG = OFF
#pragma config PWRT = OFF
#pragma config BOR = OFF
#pragma config WDT = OFF
#pragma config LVP = OFF

void main(void);

/* =======================================
 *               INTERRUPTS
 * ======================================= */
#ifdef __XC8
void __interrupt() RSI_High(void)
#else
void RSI_High(void) // For IntelliSense only
#endif
{
    if (INTCONbits.TMR0IF == 1)
    {
        Timer0_ISR();
    }
}

/* =======================================
 *               MAIN
 * ======================================= */
void main(void)
{
    // Initialize all modules in proper order
    TiInit();      // Timer system (must be first)
    SIO_Init();    // Serial communication
    LED_Init();    // PWM light control
    EEPROM_Init(); // EEPROM storage
    LCD_Init();    // LCD display
    KEY_Init();    // Keypad input
    HORA_Init();   // Time management
    RFID_Init();   // RFID card reader

    // Main cooperative loop
    while (TRUE)
    {
        // Run all module motors
        LED_Motor();  // Update PWM light control
        KEY_Motor();  // Process keypad input
        HORA_Motor(); // Update time management
        RFID_Motor(); // Update RFID motor

        // Additional motors can be added here when RFID and Controller modules are ready
        // RFID_Motor();
        // CONTROLLER_Motor();
    }
}
