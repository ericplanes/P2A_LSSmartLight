#include "TLight.h"
#include "TTimer.h"

/* =======================================
 *              CONSTANTS
 * ======================================= */

// Simple LED pin assignments (much clearer than PORT/TRIS pointers)
// LED0 -> RD1, LED1 -> RD2, LED2 -> RD3, LED3 -> RC4, LED4 -> RC5, LED5 -> RD4
#define LED0_INDEX 0
#define LED1_INDEX 1
#define LED2_INDEX 2
#define LED3_INDEX 3
#define LED4_INDEX 4
#define LED5_INDEX 5

// PWM configuration
#define MAX_TICS 10 // Maximum tics for PWM cycle (1 tic each 2ms = 50 Hz)
#define NUM_LEDS 6  // Number of LEDs to control

// LED status
#define LED_OFF 1
#define LED_ON 0

/* =======================================
 *        PRIVATE FUNCTION HEADERS
 * ======================================= */

// Helper functions - much easier to understand (Java-style)
static void configure_all_leds_as_outputs(void);
static void set_led(BYTE led_index, BYTE state);
static void update_led_pwm(BYTE led_index, BYTE brightness, WORD current_tics);

/* =======================================
 *         PRIVATE VARIABLES
 * ======================================= */

// LED configuration array (intensity level 0-10 for each LED)
static BYTE led_config[NUM_LEDS];

/* =======================================
 *         PUBLIC FUNCTION BODIES
 * ======================================= */

void LED_Init(void)
{
    // Configure all LED pins as outputs (much simpler!)
    configure_all_leds_as_outputs();

    // Initialize all LEDs to OFF (clean and simple)
    for (BYTE i = 0; i < NUM_LEDS; i++)
    {
        set_led(i, LED_OFF);
        led_config[i] = 0;
    }
}

void LED_Motor(void)
{
    static WORD current_tics;

    // Get current timer tics using TI_LIGHTS timer handle
    current_tics = TiGetTics(TI_LIGHTS);

    // Update each LED using PWM (much cleaner with loop!)
    for (BYTE i = 0; i < NUM_LEDS; i++)
    {
        update_led_pwm(i, led_config[i], current_tics);
    }

    // Reset timer every MAX_TICS for perfect 50Hz PWM
    if (current_tics >= MAX_TICS)
    {
        TiResetTics(TI_LIGHTS);
    }
}

void LED_UpdateConfig(BYTE *config)
{
    BYTE i;

    // Copy new configuration to private array
    for (i = 0; i < NUM_LEDS; i++)
    {
        // Ensure values are within valid range (0-10)
        if (config[i] > MAX_TICS)
            led_config[i] = MAX_TICS;
        else
            led_config[i] = config[i];
    }
}

/* =======================================
 *        PRIVATE FUNCTION BODIES
 * ======================================= */

static void update_led_pwm(BYTE led_index, BYTE brightness, WORD current_tics)
{
    // Simple PWM logic: LED ON when current_tics < brightness
    BYTE led_state;
    if (current_tics < brightness)
    {
        led_state = LED_ON;
    }
    else
    {
        led_state = LED_OFF;
    }
    set_led(led_index, led_state);
}

static void configure_all_leds_as_outputs(void)
{
    // Configure each LED pin as output (much clearer than bit operations!)
    TRISDbits.TRISD1 = 0; // LED0 -> RD1 output
    TRISDbits.TRISD2 = 0; // LED1 -> RD2 output
    TRISDbits.TRISD3 = 0; // LED2 -> RD3 output
    TRISCbits.TRISC4 = 0; // LED3 -> RC4 output
    TRISCbits.TRISC5 = 0; // LED4 -> RC5 output
    TRISDbits.TRISD4 = 0; // LED5 -> RD4 output
}

static void set_led(BYTE led_index, BYTE state)
{
    switch (led_index)
    {
    case LED0_INDEX:
        LATDbits.LATD1 = state; // LED0 -> RD1
        break;
    case LED1_INDEX:
        LATDbits.LATD2 = state; // LED1 -> RD2
        break;
    case LED2_INDEX:
        LATDbits.LATD3 = state; // LED2 -> RD3
        break;
    case LED3_INDEX:
        LATCbits.LATC4 = state; // LED3 -> RC4
        break;
    case LED4_INDEX:
        LATCbits.LATC5 = state; // LED4 -> RC5
        break;
    case LED5_INDEX:
        LATDbits.LATD4 = state; // LED5 -> RD4
        break;
    }
}
