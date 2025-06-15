#include "TKeypad.h"
#include "TTimer.h"

#define WAIT_16MS TWO_MS * 8
#define WAIT_3S ONE_SECOND * 3

// Special key defines
#define HASH_KEY 11       // '#' key value
#define STAR_KEY 10       // '*' key value
#define NO_KEY_PRESSED 12 // Value when no key is pressed

// Hardware configuration defines
#define TRISC_KEYPAD_CONFIG 0x0F // RC0-RC3 outputs (rows), RC4-RC7 inputs
#define TRISB_KEYPAD_CONFIG 0xFF // RB0-RB7 inputs (columns)
#define PORTB_COLUMN_MASK 0x07   // Mask for RB0-RB2 (columns)

// Column detection values
#define COL0_DETECTED 1 // RB0 high
#define COL1_DETECTED 2 // RB1 high
#define COL2_DETECTED 4 // RB2 high

// Keypad dimensions
#define KEYPAD_ROWS 4
#define KEYPAD_COLS 3

// Column indices
#define COL0_INDEX 0
#define COL1_INDEX 1
#define COL2_INDEX 2

// Row shift values for scanning
#define ROW0_SHIFT 0x01
#define ROW1_SHIFT 0x02
#define ROW2_SHIFT 0x04
#define ROW3_SHIFT 0x08

// LED limits
#define MIN_LED_NUMBER 0
#define MAX_LED_NUMBER 5

// Keypad state machine states
#define STATE_IDLE 0
#define STATE_DEBOUNCE_PRESS 1
#define STATE_DEBOUNCE_RELEASE 2
#define STATE_PROCESS_KEY 3
#define STATE_WAIT_SECOND_KEY 4
#define STATE_WAIT_RESET_CONFIRM 5

// Static variables
static BYTE keypad_state = STATE_IDLE;
static BYTE current_key = NO_KEY_PRESSED;
static BYTE old_key = NO_KEY_PRESSED;
static BYTE row_index = 0;
static BYTE command_ready = NO_COMMAND;
static BYTE led_number = 0;
static BYTE led_intensity = 0;
static BOOL waiting_for_second_key = FALSE;
static BOOL user_inside = FALSE;

// Key mapping table: [row][col] -> key_value
static const BYTE key_table[KEYPAD_ROWS][KEYPAD_COLS] = {
    {1, 2, 3},              // Row 0: 1, 2, 3
    {4, 5, 6},              // Row 1: 4, 5, 6
    {7, 8, 9},              // Row 2: 7, 8, 9
    {STAR_KEY, 0, HASH_KEY} // Row 3: *, 0, #
};

/* =======================================
 *       PRIVATE FUNCTION HEADERS
 * ======================================= */
static void scan_keypad(void);
static BYTE is_key_pressed(void);
static BYTE convert_to_key(void);
static void process_detected_key(BYTE key);
static BYTE is_valid_led_number(BYTE key);
static BYTE is_hash_key(BYTE key);
static void reset_internal_state(void);

/* =======================================
 *          PUBLIC FUNCTION BODIES
 * ======================================= */

void KEY_Init(void)
{
    // Initialize keypad hardware
    TRISC = TRISC_KEYPAD_CONFIG; // RC0-RC3 as outputs (rows), RC4-RC7 as inputs
    TRISB = TRISB_KEYPAD_CONFIG; // RB0-RB7 as inputs (columns)

    // Reset internal state
    reset_internal_state();
}

void KEY_Motor(void)
{
    switch (keypad_state)
    {
    case STATE_IDLE:
        scan_keypad();
        if (is_key_pressed() && user_inside)
        {
            TiResetTics(TI_KEYPAD);
            keypad_state = STATE_DEBOUNCE_PRESS;
        }
        break;

    case STATE_DEBOUNCE_PRESS:
        if (TiGetTics(TI_KEYPAD) >= WAIT_16MS)
        {
            // Go to idle state first
            keypad_state = STATE_IDLE;

            // Check if a key is pressed
            if (is_key_pressed())
            {
                current_key = convert_to_key();

                // Filter out hardware errors early
                if (current_key != NO_KEY_PRESSED)
                {
                    TiResetTics(TI_KEYPAD);

                    // If no '#' key, go to debounce release state
                    keypad_state = STATE_DEBOUNCE_RELEASE;

                    // Check for '#' key first, regardless of state
                    if (is_hash_key(current_key))
                    {
                        waiting_for_second_key = FALSE; // Cancel any waiting state
                        keypad_state = STATE_WAIT_RESET_CONFIRM;
                    }
                }
                // If hardware error (NO_KEY_PRESSED), stay in current state to retry
            }
        }
        break;

    case STATE_DEBOUNCE_RELEASE:
        if (TiGetTics(TI_KEYPAD) >= WAIT_16MS)
        {
            // If key is still pressed, go back to idle to re-scan
            keypad_state = is_key_pressed() ? STATE_IDLE : STATE_PROCESS_KEY;
        }
        break;

    case STATE_PROCESS_KEY:
        process_detected_key(current_key);
        current_key = NO_KEY_PRESSED;
        keypad_state = STATE_IDLE;
        break;

    case STATE_WAIT_SECOND_KEY:
        scan_keypad();
        if (is_key_pressed())
        {
            TiResetTics(TI_KEYPAD);
            keypad_state = STATE_DEBOUNCE_PRESS;
        }
        break;

    case STATE_WAIT_RESET_CONFIRM:
        if (!is_key_pressed())
        {
            // Reset cancelled - back to normal operation
            waiting_for_second_key = FALSE;
            keypad_state = STATE_IDLE;
        }
        else if (TiGetTics(TI_KEYPAD) >= WAIT_3S)
        {
            // Reset confirmed - trigger reset and back to normal operation
            command_ready = KEYPAD_RESET;
            waiting_for_second_key = FALSE;
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

void KEY_SetUserInside(BOOL inside)
{
    if (!inside)
    {
        reset_internal_state();
    }
    user_inside = inside;
}

/* =======================================
 *          PRIVATE FUNCTION BODIES
 * ======================================= */

static void scan_keypad(void)
{
    static BYTE shift_values[] = {ROW0_SHIFT, ROW1_SHIFT, ROW2_SHIFT, ROW3_SHIFT}; // Cycle through rows
    row_index = (row_index + 1) % KEYPAD_ROWS;
    LATC = shift_values[row_index];
}

static BYTE is_key_pressed(void)
{
    return (PORTB & PORTB_COLUMN_MASK) != 0; // Check if any of RB0-RB2 is high
}

static BYTE convert_to_key(void)
{
    BYTE col_index = PORTB & PORTB_COLUMN_MASK; // Read RB0-RB2

    // Convert column reading to index
    switch (col_index)
    {
    case COL0_DETECTED:
        col_index = COL0_INDEX;
        break;
    case COL1_DETECTED:
        col_index = COL1_INDEX;
        break;
    case COL2_DETECTED:
        col_index = COL2_INDEX;
        break;
    default:
        return NO_KEY_PRESSED; // Invalid key
    }

    return key_table[row_index][col_index];
}

static void process_detected_key(BYTE key)
{
    if (waiting_for_second_key)
    {
        // Processing second key (intensity)
        led_intensity = key;
        command_ready = UPDATE_LED;
        waiting_for_second_key = FALSE; // Reset flag
        return;
    }

    // Processing first key (LED number)
    if (is_valid_led_number(key))
    {
        led_number = key;
        waiting_for_second_key = TRUE; // Set flag to wait for second key
        keypad_state = STATE_WAIT_SECOND_KEY;
    }
}

static BYTE is_valid_led_number(BYTE key)
{
    return (MIN_LED_NUMBER <= key && key <= MAX_LED_NUMBER);
}

static BYTE is_hash_key(BYTE key)
{
    return (key == HASH_KEY);
}

static void reset_internal_state(void)
{
    // Reset all state variables to initial conditions
    keypad_state = STATE_IDLE;
    current_key = NO_KEY_PRESSED;
    old_key = NO_KEY_PRESSED;
    row_index = 0;
    command_ready = NO_COMMAND;
    led_number = 0;
    led_intensity = 0;
    waiting_for_second_key = FALSE;
}
