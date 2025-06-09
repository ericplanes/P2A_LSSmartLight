#include <xc.h>
#include <pic18f4321.h>

#include "Utils.h"

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
    // Interrupt service routine
    // TODO: Add interrupt handling for smart light functionality
}

void main(void)
{
    // Set internal oscillator to 8 MHz
    OSCCONbits.IRCF = 0b111; // IRCF = 111: 8 MHz
    OSCTUNEbits.PLLEN = 1;   // Enable 4x PLL => 32 MHz
    OSCCONbits.SCS = 0b00;   // Use clock defined by CONFIG

    // TODO: Initialize smart light subsystems
    // Examples:
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