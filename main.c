#include <xc.h>
#include <pic18f4321.h>

#include "Utils.h"
#include "TTimer.h"
#include "TEEPROM.h"

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

void __interrupt() RSI_High(void)
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

    // TODO: Initialize remaining subsystems
    // LED_Init();
    // PWM_Init();
    // SensorInput_Init();
    // Communication_Init();

    while (TRUE)
    {
        // Main loop for smart light functionality
        // TODO: Add smart light motor functions
        // Examples:
        // LightControl_Motor();
        // SensorInput_Motor();
        // Communication_Motor();

        // Placeholder for now
        __delay_ms(100);
    }
}
