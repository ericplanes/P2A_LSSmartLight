#include <xc.h>
#include <pic18f4321.h>

#include "Utils.h"
#include "TTimer.h"
#include "TEEPROM.h"
#include "TLCD.h"
#include "TKeypad.h"
#include "THora.h"

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
    // Initialize all modules
    TiInit();
    EEPROM_Init();
    LCD_Init();
    KEY_Init();
    HORA_Init();

    // Main cooperative loop
    while (TRUE)
    {
        // Run all module motors
        KEY_Motor();
        HORA_Motor();
    }
}
