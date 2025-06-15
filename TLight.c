#include "TLight.h"
#include "TTimer.h"

/* =======================================
 *              CONSTANTS
 * ======================================= */

// LED port and pin assignments (easily modifiable)
#define LED0_PORT PORTA
#define LED0_TRIS TRISA
#define LED0_PIN 2 // PORTA bit 2 (RA2)

#define LED1_PORT PORTA
#define LED1_TRIS TRISA
#define LED1_PIN 3 // PORTA bit 3 (RA3)

#define LED2_PORT PORTA
#define LED2_TRIS TRISA
#define LED2_PIN 4 // PORTA bit 4 (RA4)

#define LED3_PORT PORTA
#define LED3_TRIS TRISA
#define LED3_PIN 5 // PORTA bit 5 (RA5)

#define LED4_PORT PORTA
#define LED4_TRIS TRISA
#define LED4_PIN 6 // PORTA bit 6 (RA6)

#define LED5_PORT PORTA
#define LED5_TRIS TRISA
#define LED5_PIN 7 // PORTA bit 7 (RA7)

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
    configure_led_as_output(&LED0_TRIS, LED0_PIN); // LED0 output
    configure_led_as_output(&LED1_TRIS, LED1_PIN); // LED1 output
    configure_led_as_output(&LED2_TRIS, LED2_PIN); // LED2 output
    configure_led_as_output(&LED3_TRIS, LED3_PIN); // LED3 output
    configure_led_as_output(&LED4_TRIS, LED4_PIN); // LED4 output
    configure_led_as_output(&LED5_TRIS, LED5_PIN); // LED5 output

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
