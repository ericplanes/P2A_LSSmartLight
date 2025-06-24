#include <xc.h>
#include <pic18f4321.h>

#include "Utils.h"
#include "TTimer.h"
#include "TSerial.h"
#include "TLight.h"
#include "TEEPROM.h"
#include "TLCD.h"
#include "TKeypad.h"
#include "THora.h"
#include "TRFID.h"
#include "TController.h"

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
        LED_Motor();
    }
}

void init_users(void)
{
    while (!EEPROM_StoreConfigForUser(0, (BYTE[]){1, 2, 3, 4, 5, 6}))
        ;
    while (!EEPROM_StoreConfigForUser(2, (BYTE[]){7, 8, 9, 10, 0, 1}))
        ;
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
    CNTR_Init();   // Main system controller, has to be the last one

    init_users();

    // Main cooperative loop
    while (TRUE)
    {
        // Run all hardware module motors
        KEY_Motor();  // Process keypad input
        HORA_Motor(); // Update time management
        RFID_Motor(); // Update RFID motor

        // Run main system controller
        CNTR_Motor(); // Coordinate all system logic
    }
}
