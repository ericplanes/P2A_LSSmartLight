#include "TKeypad.h"
#include "TTimer.h"
#include "TSerial.h"

#define WAIT_16MS TWO_MS * 8
#define WAIT_3S ONE_SECOND * 3

// Special key defines
#define HASH_KEY 11       // '#' key value
#define STAR_KEY 10       // '*' key value
#define NO_KEY_PRESSED 12 // Value when no key is pressed

// New pin assignments for keypad matrix (PORTA)
// Columns (inputs): C0=A2, C1=A0, C2=A4
// Rows (outputs):   F0=A1, F1=A6, F2=A5, F3=A3

// Hardware configuration - ACTIVE HIGH logic (1=active, 0=inactive)
// Columns = outputs (we control these), Rows = inputs (we read these)
// A0: C1 (output)  -> bit 0 = 0 (output)
// A1: F0 (input)   -> bit 1 = 1 (input)
// A2: C0 (output)  -> bit 2 = 0 (output)
// A3: F3 (input)   -> bit 3 = 1 (input)
// A4: C2 (output)  -> bit 4 = 0 (output)
// A5: F2 (input)   -> bit 5 = 1 (input)
// A6: F1 (input)   -> bit 6 = 1 (input)
// A7: unused       -> bit 7 = 1 (input for safety)

// Simple pin assignments (easier to understand than hex masks)
// Column pins (outputs - we set these to scan)
#define COL0_PIN_BIT 2 // A2
#define COL1_PIN_BIT 0 // A0
#define COL2_PIN_BIT 4 // A4

// Row pins (inputs - we read these to detect key press)
#define ROW0_PIN_BIT 1 // A1
#define ROW1_PIN_BIT 6 // A6
#define ROW2_PIN_BIT 5 // A5
#define ROW3_PIN_BIT 3 // A3

// Pin direction configuration (1=input, 0=output)
// Binary: 11101010 = 0xEA (A1,A3,A5,A6,A7 inputs, A0,A2,A4 outputs)
#define TRISA_KEYPAD_CONFIG 0xEA

// Keypad layout
#define KEYPAD_ROWS 4
#define KEYPAD_COLS 3

// Column indices (which column we're currently scanning)
#define COL0_INDEX 0 // C0 -> A2
#define COL1_INDEX 1 // C1 -> A0
#define COL2_INDEX 2 // C2 -> A4

// Row indices (which row we detected as pressed)
#define ROW0_INDEX 0 // F0 -> A1
#define ROW1_INDEX 1 // F1 -> A6
#define ROW2_INDEX 2 // F2 -> A5
#define ROW3_INDEX 3 // F3 -> A3

// Helper functions to make code more readable (like Java getters/setters)
static void set_column_active(BYTE col_index);
static void set_all_columns_inactive(void);
static BOOL is_row_pressed(BYTE row_bit);
static BYTE get_pressed_row(void);

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
static BYTE col_index = 0;
static BYTE command_ready = NO_COMMAND;
static BYTE led_number = 0;
static BYTE led_intensity = 0;
static BOOL waiting_for_second_key = FALSE;
static BOOL user_inside = FALSE;
static BOOL scanning_paused = FALSE; // Para l'escombrat quan hi ha tecla premuda

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
    // A1,A3,A5,A6,A7 as inputs (rows + safety)
    // A0,A2,A4 as outputs (columns)
    TRISA = TRISA_KEYPAD_CONFIG;

    // Set all column pins to inactive state (LOW = inactive with new logic)
    set_all_columns_inactive();

    // Reset internal state
    reset_internal_state();
}

void KEY_Motor(void)
{
    switch (keypad_state)
    {
    case STATE_IDLE:
        // Only scan if no key is currently pressed (avoid interference)
        if (!scanning_paused)
        {
            scan_keypad();
        }

        if (is_key_pressed() && user_inside)
        {
            SIO_TEST_SendString("KEY_DETECTED_IDLE\r\n");
            scanning_paused = TRUE; // Stop scanning while processing key
            TiResetTics(TI_KEYPAD);
            keypad_state = STATE_DEBOUNCE_PRESS;
        }
        else if (scanning_paused && !is_key_pressed())
        {
            // Resume scanning when no key is pressed
            SIO_TEST_SendString("RESUMING_SCAN\r\n");
            scanning_paused = FALSE;
        }
        break;

    case STATE_DEBOUNCE_PRESS:
        if (TiGetTics(TI_KEYPAD) >= WAIT_16MS)
        {
            SIO_TEST_SendString("DEBOUNCE_PRESS_TIMEOUT\r\n");

            // Check if a key is still pressed after debounce
            if (is_key_pressed())
            {
                current_key = convert_to_key();
                SIO_TEST_SendString("KEY_STILL_PRESSED: ");

                // Send key value for debugging
                BYTE key_str[3];
                key_str[0] = (current_key < 10) ? ('0' + current_key) : ('A' + current_key - 10);
                key_str[1] = '\r';
                key_str[2] = '\n';
                SIO_TEST_SendString(key_str);

                // Filter out hardware errors early
                if (current_key != NO_KEY_PRESSED)
                {
                    SIO_TEST_SendString("KEY_VALID\r\n");
                    TiResetTics(TI_KEYPAD);

                    // Check for '#' key first, regardless of state
                    if (is_hash_key(current_key))
                    {
                        SIO_TEST_SendString("HASH_KEY_DETECTED\r\n");
                        waiting_for_second_key = FALSE; // Cancel any waiting state
                        keypad_state = STATE_WAIT_RESET_CONFIRM;
                    }
                    else
                    {
                        keypad_state = STATE_DEBOUNCE_RELEASE;
                    }
                }
                else
                {
                    SIO_TEST_SendString("KEY_INVALID\r\n");
                    keypad_state = STATE_IDLE; // Back to idle if invalid
                }
            }
            else
            {
                SIO_TEST_SendString("KEY_NO_LONGER_PRESSED\r\n");
                keypad_state = STATE_IDLE; // Back to idle if no key
            }
        }
        break;

    case STATE_DEBOUNCE_RELEASE:
        if (TiGetTics(TI_KEYPAD) >= WAIT_16MS)
        {
            if (is_key_pressed())
            {
                SIO_TEST_SendString("KEY_STILL_PRESSED_AFTER_RELEASE_DEBOUNCE\r\n");
                keypad_state = STATE_IDLE; // Back to idle to re-scan
            }
            else
            {
                SIO_TEST_SendString("KEY_RELEASED_CONFIRMED\r\n");
                keypad_state = STATE_PROCESS_KEY;
            }
        }
        break;

    case STATE_PROCESS_KEY:
        SIO_TEST_SendString("PROCESSING_KEY\r\n");
        process_detected_key(current_key);
        current_key = NO_KEY_PRESSED;
        scanning_paused = FALSE; // Resume scanning after processing
        keypad_state = STATE_IDLE;
        break;

    case STATE_WAIT_SECOND_KEY:
        // Continue scanning while waiting for second key
        if (!scanning_paused)
        {
            scan_keypad();
        }

        if (is_key_pressed())
        {
            SIO_TEST_SendString("SECOND_KEY_DETECTED\r\n");
            scanning_paused = TRUE; // Stop scanning while processing
            TiResetTics(TI_KEYPAD);
            keypad_state = STATE_DEBOUNCE_PRESS;
        }
        else if (scanning_paused && !is_key_pressed())
        {
            // Resume scanning if it was paused
            SIO_TEST_SendString("RESUMING_SCAN_WAIT_SECOND\r\n");
            scanning_paused = FALSE;
        }
        break;

    case STATE_WAIT_RESET_CONFIRM:
        if (!is_key_pressed())
        {
            SIO_TEST_SendString("RESET_CANCELLED\r\n");
            waiting_for_second_key = FALSE;
            scanning_paused = FALSE; // Resume scanning
            keypad_state = STATE_IDLE;
        }
        else if (TiGetTics(TI_KEYPAD) >= WAIT_3S)
        {
            SIO_TEST_SendString("RESET_CONFIRMED\r\n");
            command_ready = KEYPAD_RESET;
            waiting_for_second_key = FALSE;
            scanning_paused = FALSE; // Resume scanning
            keypad_state = STATE_IDLE;
        }
        break;
    }
}

BYTE KEY_GetCommand(void)
{
    BYTE cmd = command_ready;
    command_ready = NO_COMMAND;
    return cmd;
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
    // Cycle through columns (C0, C1, C2)
    col_index = (col_index + 1) % KEYPAD_COLS;

    // Debug: show which column we're scanning
    BYTE col_str[10];
    col_str[0] = 'C';
    col_str[1] = 'O';
    col_str[2] = 'L';
    col_str[3] = ':';
    col_str[4] = '0' + col_index;
    col_str[5] = '\r';
    col_str[6] = '\n';
    col_str[7] = '\0';
    SIO_TEST_SendString(col_str);

    // First, set all columns to inactive (easier to understand than bit masks)
    set_all_columns_inactive();

    // Then activate the current column (much clearer than hex operations)
    set_column_active(col_index);
}

static BYTE is_key_pressed(void)
{
    // Check if any row pin is HIGH (pressed key connects active column to row)
    // ACTIVE HIGH logic: when key pressed, row goes HIGH
    return get_pressed_row() != NO_KEY_PRESSED;
}

static BYTE convert_to_key(void)
{
    // Get which row is currently pressed (much simpler than bit operations)
    BYTE row_index = get_pressed_row();

    if (row_index == NO_KEY_PRESSED)
    {
        return NO_KEY_PRESSED; // No valid row detected
    }

    // Calculate key value based on row and column
    // col_index tells us which column is currently active
    if (row_index == 3) // Bottom row: *, 0, #
    {
        if (col_index == 0)
            return STAR_KEY;
        if (col_index == 1)
            return 0;
        return HASH_KEY;
    }
    else // Rows 0-2: standard numeric layout (1-9)
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
    col_index = 0;
    command_ready = NO_COMMAND;
    led_number = 0;
    led_intensity = 0;
    waiting_for_second_key = FALSE;
    scanning_paused = FALSE; // Resume scanning on reset
    SIO_TEST_SendString("KEYPAD_STATE_RESET\r\n");
}

// Helper functions - much easier to understand than hex masks (Java-style)
static void set_column_active(BYTE col_index)
{
    // ACTIVE HIGH: Set the specific column pin to HIGH (1) to activate it
    switch (col_index)
    {
    case COL0_INDEX:
        LATAbits.LATA2 = 1; // A2 = COL0 active
        break;
    case COL1_INDEX:
        LATAbits.LATA0 = 1; // A0 = COL1 active
        break;
    case COL2_INDEX:
        LATAbits.LATA4 = 1; // A4 = COL2 active
        break;
    }
}

static void set_all_columns_inactive(void)
{
    // ACTIVE HIGH: Set all column pins to LOW (0) to deactivate them
    LATAbits.LATA2 = 0; // COL0 inactive
    LATAbits.LATA0 = 0; // COL1 inactive
    LATAbits.LATA4 = 0; // COL2 inactive
}

static BOOL is_row_pressed(BYTE row_bit)
{
    // ACTIVE HIGH: Check if specific row pin is HIGH (1) = pressed
    switch (row_bit)
    {
    case ROW0_PIN_BIT:
        return PORTAbits.RA1; // A1 = ROW0
    case ROW1_PIN_BIT:
        return PORTAbits.RA6; // A6 = ROW1
    case ROW2_PIN_BIT:
        return PORTAbits.RA5; // A5 = ROW2
    case ROW3_PIN_BIT:
        return PORTAbits.RA3; // A3 = ROW3
    default:
        return FALSE;
    }
}

static BYTE get_pressed_row(void)
{
    // Check each row individually (much clearer than bit operations)
    if (is_row_pressed(ROW0_PIN_BIT))
        return ROW0_INDEX;
    if (is_row_pressed(ROW1_PIN_BIT))
        return ROW1_INDEX;
    if (is_row_pressed(ROW2_PIN_BIT))
        return ROW2_INDEX;
    if (is_row_pressed(ROW3_PIN_BIT))
        return ROW3_INDEX;

    return NO_KEY_PRESSED; // No row pressed
}
