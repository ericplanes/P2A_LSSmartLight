#include "TKeypad.h"
#include "TTimer.h"

#define WAIT_16MS TWO_MS * 8
#define WAIT_3S ONE_SECOND * 3

// Keypad state machine states
#define STATE_IDLE 0
#define STATE_DEBOUNCE_PRESS 1
#define STATE_DEBOUNCE_RELEASE 2
#define STATE_PROCESS_KEY 3
#define STATE_WAIT_SECOND_KEY 4
#define STATE_WAIT_RESET_CONFIRM 5

// Static variables
static BYTE timer_handle;
static BYTE keypad_state = STATE_IDLE;
static BYTE current_key = 12; // 12 = no key pressed
static BYTE old_key = 12;
static BYTE row_index = 0;
static BYTE command_ready = NO_COMMAND;
static BYTE led_number = 0;
static BYTE led_intensity = 0;

// Key mapping table: [row][col] -> key_value
static const BYTE key_table[4][3] = {
    {1, 2, 3},  // Row 0: 1, 2, 3
    {4, 5, 6},  // Row 1: 4, 5, 6
    {7, 8, 9},  // Row 2: 7, 8, 9
    {10, 0, 11} // Row 3: *, 0, #
};

/* =======================================
 *       PRIVATE FUNCTION HEADERS
 * ======================================= */
static void scan_keypad(void);
static BYTE is_key_pressed(void);
static BYTE convert_to_key(void);
static void process_detected_key(BYTE key);
static BYTE is_valid_led_number(BYTE key);
static BYTE is_valid_intensity(BYTE key);

/* =======================================
 *          PUBLIC FUNCTION BODIES
 * ======================================= */

void KEY_Init(void)
{
    // Initialize timer
    TiNewTimer(&timer_handle);

    // Initialize keypad hardware
    TRISC = 0x0F; // RC0-RC3 as outputs (rows), RC4-RC7 as inputs
    TRISB = 0xFF; // RB0-RB7 as inputs (columns)

    // Initialize state
    keypad_state = STATE_IDLE;
    current_key = 12;
    old_key = 12;
    row_index = 0;
    command_ready = NO_COMMAND;

    // Start scanning
    scan_keypad();
}

void KEY_Motor(void)
{
    switch (keypad_state)
    {
    case STATE_IDLE:
        scan_keypad();
        if (is_key_pressed())
        {
            TiResetTics(timer_handle);
            keypad_state = STATE_DEBOUNCE_PRESS;
        }
        break;

    case STATE_DEBOUNCE_PRESS:
        if (TiGetTics(timer_handle) >= WAIT_16MS)
        {
            if (is_key_pressed())
            {
                current_key = convert_to_key();
                TiResetTics(timer_handle);
                keypad_state = STATE_DEBOUNCE_RELEASE;
            }
            else
            {
                keypad_state = STATE_IDLE;
            }
        }
        break;

    case STATE_DEBOUNCE_RELEASE:
        if (TiGetTics(timer_handle) >= WAIT_16MS)
        {
            if (!is_key_pressed())
            {
                keypad_state = STATE_PROCESS_KEY;
            }
            else if (current_key == 11) // '#' key
            {
                // Start 3-second timer for reset
                TiResetTics(timer_handle);
                keypad_state = STATE_WAIT_RESET_CONFIRM;
            }
            else
            {
                keypad_state = STATE_IDLE;
            }
        }
        break;

    case STATE_PROCESS_KEY:
        process_detected_key(current_key);
        current_key = 12;
        keypad_state = STATE_IDLE;
        break;

    case STATE_WAIT_SECOND_KEY:
        scan_keypad();
        if (is_key_pressed())
        {
            TiResetTics(timer_handle);
            keypad_state = STATE_DEBOUNCE_PRESS;
        }
        break;

    case STATE_WAIT_RESET_CONFIRM:
        if (!is_key_pressed())
        {
            keypad_state = STATE_IDLE;
        }
        else if (TiGetTics(timer_handle) >= WAIT_3S)
        {
            command_ready = RESET;
            keypad_state = STATE_IDLE;
        }
        break;
    }
}

BYTE KEY_GetCommand(void)
{
    BYTE result = command_ready;
    command_ready = NO_COMMAND;
    return result;
}

void KEY_GetUpdateInfo(BYTE *led, BYTE *intensity)
{
    *led = led_number;
    *intensity = led_intensity;
}

/* =======================================
 *          PRIVATE FUNCTION BODIES
 * ======================================= */

static void scan_keypad(void)
{
    // Cycle through rows (0-3)
    static BYTE shift_values[] = {0x01, 0x02, 0x04, 0x08};

    row_index = (row_index + 1) % 4;
    LATC = shift_values[row_index];
}

static BYTE is_key_pressed(void)
{
    return (PORTB & 0x07) != 0; // Check if any of RB0-RB2 is high
}

static BYTE convert_to_key(void)
{
    BYTE col_index = PORTB & 0x07; // Read RB0-RB2

    // Convert column reading to index (1,2,4 -> 0,1,2)
    if (col_index == 1)
        col_index = 0;
    else if (col_index == 2)
        col_index = 1;
    else if (col_index == 4)
        col_index = 2;
    else
        return 12; // Invalid key

    return key_table[row_index][col_index];
}

static void process_detected_key(BYTE key)
{
    if (keypad_state == STATE_WAIT_SECOND_KEY)
    {
        if (is_valid_intensity(key))
        {
            led_intensity = key;
            command_ready = UPDATE_LED;
        }
        return; // If invalid intensity, ignore and continue waiting
    }

    // Processing first key
    if (is_valid_led_number(key))
    {
        led_number = key;
        keypad_state = STATE_WAIT_SECOND_KEY;
    }
    // Note: '#' key (11) is handled in STATE_DEBOUNCE_RELEASE
}

static BYTE is_valid_led_number(BYTE key)
{
    return (0 <= key && key <= 5);
}

static BYTE is_valid_intensity(BYTE key)
{
    return (0 <= key && key <= 10);
}
