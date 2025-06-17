#include "TLight.h"
#include "TTimer.h"

/* =======================================
 *              CONSTANTS
 * ======================================= */

// New LED pin assignments: LED0->RD1, LED1->RD2, LED2->RD3, LED3->RC4, LED4->RC5, LED5->RD4
#define LED0_PORT PORTD
#define LED0_TRIS TRISD
#define LED0_PIN 1 // PORTD bit 1 (RD1)

#define LED1_PORT PORTD
#define LED1_TRIS TRISD
#define LED1_PIN 2 // PORTD bit 2 (RD2)

#define LED2_PORT PORTD
#define LED2_TRIS TRISD
#define LED2_PIN 3 // PORTD bit 3 (RD3)

#define LED3_PORT PORTC
#define LED3_TRIS TRISC
#define LED3_PIN 4 // PORTC bit 4 (RC4)

#define LED4_PORT PORTC
#define LED4_TRIS TRISC
#define LED4_PIN 5 // PORTC bit 5 (RC5)

#define LED5_PORT PORTD
#define LED5_TRIS TRISD
#define LED5_PIN 4 // PORTD bit 4 (RD4)

// PWM configuration
#define MAX_TICS 10 // Maximum tics for PWM cycle (1 tic each 2ms = 50 Hz)
#define NUM_LEDS 6  // Number of LEDs to control

// LED status
#define LED_OFF FALSE
#define LED_ON TRUE

/* =======================================
 *        PRIVATE FUNCTION HEADERS
 * ======================================= */

static void configure_led_as_output(volatile BYTE *tris_reg, BYTE pin);
static void set_led_state(BOOL state, volatile BYTE *port_reg, BYTE pin);
static void update_led_pwm(BYTE brightness, WORD current_tics, volatile BYTE *port_reg, BYTE pin);

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
    // Configure LED pins as outputs using helper function
    configure_led_as_output(&LED0_TRIS, LED0_PIN); // LED0 -> RD1 output
    configure_led_as_output(&LED1_TRIS, LED1_PIN); // LED1 -> RD2 output
    configure_led_as_output(&LED2_TRIS, LED2_PIN); // LED2 -> RD3 output
    configure_led_as_output(&LED3_TRIS, LED3_PIN); // LED3 -> RC4 output
    configure_led_as_output(&LED4_TRIS, LED4_PIN); // LED4 -> RC5 output
    configure_led_as_output(&LED5_TRIS, LED5_PIN); // LED5 -> RD4 output

    // Initialize all LEDs to OFF using helper function
    set_led_state(LED_OFF, &LED0_PORT, LED0_PIN); // LED0 OFF
    set_led_state(LED_OFF, &LED1_PORT, LED1_PIN); // LED1 OFF
    set_led_state(LED_OFF, &LED2_PORT, LED2_PIN); // LED2 OFF
    set_led_state(LED_OFF, &LED3_PORT, LED3_PIN); // LED3 OFF
    set_led_state(LED_OFF, &LED4_PORT, LED4_PIN); // LED4 OFF
    set_led_state(LED_OFF, &LED5_PORT, LED5_PIN); // LED5 OFF

    // Initialize configuration array to all zeros (all LEDs OFF)
    for (BYTE i = 0; i < NUM_LEDS; i++)
    {
        led_config[i] = 0;
    }
}

void LED_Motor(void)
{
    static WORD current_tics;

    // Get current timer tics using TI_LIGHTS timer handle
    current_tics = TiGetTics(TI_LIGHTS);

    // Update each LED using PWM helper function
    update_led_pwm(led_config[0], current_tics, &LED0_PORT, LED0_PIN);
    update_led_pwm(led_config[1], current_tics, &LED1_PORT, LED1_PIN);
    update_led_pwm(led_config[2], current_tics, &LED2_PORT, LED2_PIN);
    update_led_pwm(led_config[3], current_tics, &LED3_PORT, LED3_PIN);
    update_led_pwm(led_config[4], current_tics, &LED4_PORT, LED4_PIN);
    update_led_pwm(led_config[5], current_tics, &LED5_PORT, LED5_PIN);

    // Check reset tics - reset every MAX_TICS for perfect 50Hz PWM
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

static void update_led_pwm(BYTE brightness, WORD current_tics, volatile BYTE *port_reg, BYTE pin)
{
    current_tics < brightness ? set_led_state(LED_ON, port_reg, pin) : set_led_state(LED_OFF, port_reg, pin);
}

static void configure_led_as_output(volatile BYTE *tris_reg, BYTE pin)
{
    // Configure LED pin as output (clear TRIS bit)
    *tris_reg &= ~(1 << pin);
}

static void set_led_state(BOOL state, volatile BYTE *port_reg, BYTE pin)
{
    // ~ is bitwise NOT, to only modify the specific pin from the port
    *port_reg = state == LED_ON ? (BYTE)(*port_reg | (1 << pin)) : (BYTE)(*port_reg & ~(1 << pin));
}
