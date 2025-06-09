#include <xc.h>
#include <pic18f4321.h>

#include "Utils.h"
#include "TTimer.h"
#include "TEEPROM.h"
#include "TLCD.h"

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
 *               MAIN
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

void main(void)
{
    // Initialize smart light subsystems
    TiInit();
    EEPROM_Init();

    while (TRUE)
    {
    }
}
