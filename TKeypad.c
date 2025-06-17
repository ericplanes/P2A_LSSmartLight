#include "TKeypad.h"
#include "TTimer.h"

#define WAIT_16MS TWO_MS * 8
#define WAIT_3S ONE_SECOND * 3

// Special key defines
#define HASH_KEY 11       // '#' key value
#define STAR_KEY 10       // '*' key value
#define NO_KEY_PRESSED 12 // Value when no key is pressed

// New pin assignments for keypad matrix (PORTA)
// Columns (inputs): C0=A2, C1=A0, C2=A4
// Rows (outputs):   F0=A1, F1=A6, F2=A5, F3=A3

// Hardware configuration defines
#define TRISA_KEYPAD_ROWS 0x59 // A0,A2,A4 as inputs (columns), A1,A3,A5,A6 as outputs (rows)
                               // Binary: 01011001 (bits 0,2,4,6 inputs, bits 1,3,5,7 outputs)
                               // Wait, that's wrong. Let me fix this

// Actually, let me recalculate:
// A0: C1 (input)   -> bit 0 = 1
// A1: F0 (output)  -> bit 1 = 0
// A2: C0 (input)   -> bit 2 = 1
// A3: F3 (output)  -> bit 3 = 0
// A4: C2 (input)   -> bit 4 = 1
// A5: F2 (output)  -> bit 5 = 0
// A6: F1 (output)  -> bit 6 = 0
// A7: unused       -> bit 7 = 1 (input for safety)
#define TRISA_KEYPAD_CONFIG 0x95 // A0,A2,A4,A7 as inputs (columns+safety), A1,A3,A5,A6 as outputs (rows)

// Column pin masks (inputs)
#define COL0_PIN_MASK 0x04 // A2 (C0)
#define COL1_PIN_MASK 0x01 // A0 (C1)
#define COL2_PIN_MASK 0x10 // A4 (C2)
#define COLUMNS_MASK (COL0_PIN_MASK | COL1_PIN_MASK | COL2_PIN_MASK)

// Row pin positions (outputs)
#define ROW0_PIN 0x02 // A1 (F0)
#define ROW1_PIN 0x40 // A6 (F1)
#define ROW2_PIN 0x20 // A5 (F2)
#define ROW3_PIN 0x08 // A3 (F3)
#define ROWS_MASK (ROW0_PIN | ROW1_PIN | ROW2_PIN | ROW3_PIN)

// Keypad dimensions
#define KEYPAD_ROWS 4
#define KEYPAD_COLS 3

// Column indices
#define COL0_INDEX 0 // C0 -> A2
#define COL1_INDEX 1 // C1 -> A0
#define COL2_INDEX 2 // C2 -> A4

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

// Optimized: Arrays replaced with inline calculations to save memory

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
    // Initialize keypad hardware on PORT A
    // A0,A2,A4,A7 as inputs (columns + safety)
    // A1,A3,A5,A6 as outputs (rows)
    TRISA = TRISA_KEYPAD_CONFIG;

    // Set all row pins HIGH initially (inactive state)
    LATA |= ROWS_MASK;

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
    return command_ready;
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

void KEY_ResetCommand(void)
{
    command_ready = NO_COMMAND;
}

/* =======================================
 *          PRIVATE FUNCTION BODIES
 * ======================================= */

static void scan_keypad(void)
{
    BYTE current_row_pin;

    // Cycle through rows (F0, F1, F2, F3)
    row_index = (row_index + 1) % KEYPAD_ROWS;

    // Calculate row pin inline to save memory
    switch (row_index)
    {
    case 0:
        current_row_pin = ROW0_PIN;
        break;
    case 1:
        current_row_pin = ROW1_PIN;
        break;
    case 2:
        current_row_pin = ROW2_PIN;
        break;
    default:
        current_row_pin = ROW3_PIN;
        break;
    }

    // Set all row pins HIGH first (inactive)
    LATA |= ROWS_MASK;

    // Set current row pin LOW to activate it
    LATA &= ~current_row_pin;
}

static BYTE is_key_pressed(void)
{
    // Check if any column pin is LOW (pressed key pulls column down)
    // When a key is pressed, the active row (LOW) connects to the column
    return (PORTA & COLUMNS_MASK) != COLUMNS_MASK;
}

static BYTE convert_to_key(void)
{
    BYTE columns_state = PORTA & COLUMNS_MASK;
    BYTE col_index;

    // Determine which column is pressed (LOW)
    if (!(columns_state & COL0_PIN_MASK)) // C0 (A2) is LOW
    {
        col_index = COL0_INDEX;
    }
    else if (!(columns_state & COL1_PIN_MASK)) // C1 (A0) is LOW
    {
        col_index = COL1_INDEX;
    }
    else if (!(columns_state & COL2_PIN_MASK)) // C2 (A4) is LOW
    {
        col_index = COL2_INDEX;
    }
    else
    {
        return NO_KEY_PRESSED; // No valid column detected
    }

    // Calculate key value inline to save memory
    if (row_index == 3) // Bottom row: *, 0, #
    {
        if (col_index == 0)
            return STAR_KEY;
        if (col_index == 1)
            return 0;
        return HASH_KEY;
    }
    else // Rows 0-2: standard numeric layout
    {
        return (row_index * 3) + col_index + 1;
    }
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
    return (key <= MAX_LED_NUMBER);
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
