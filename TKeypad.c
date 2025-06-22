#include "TKeypad.h"
#include "TTimer.h"
#include "TSerial.h"

#define WAIT_16MS TWO_MS * 8
#define WAIT_3S ONE_SECOND * 3

// Special key defines
#define ZERO_KEY 11
#define HASH_KEY 12
#define NO_KEY_PRESSED 13

// Pin assignments (PORTA)
#define ROW0_PIN_BIT 1 // A1
#define ROW1_PIN_BIT 6 // A6
#define ROW2_PIN_BIT 5 // A5
#define ROW3_PIN_BIT 3 // A3

#define KEYPAD_ROWS 4
#define KEYPAD_COLS 3
#define COL0_INDEX 0
#define COL1_INDEX 1
#define COL2_INDEX 2
#define ROW0_INDEX 0
#define ROW1_INDEX 1
#define ROW2_INDEX 2
#define ROW3_INDEX 3

#define MIN_LED_NUMBER 0
#define MAX_LED_NUMBER 5

// Simplified state names
#define IDLE 0
#define ON_KEY_PRESS 1
#define READ_KEY_VALUE 2
#define CHECK_KEY_VALUE 3
#define STORE_KEY 4
#define RESET_HOLD 5
#define ON_KEY_RELEASE 6
#define WAIT_FOR_RELEASE 7

static BYTE keypad_state;
static BYTE current_key;
static BYTE col_index;
static BYTE command_ready;
static BYTE led_number;
static BYTE led_intensity;
static BOOL waiting_for_second_key;
static BOOL user_inside;

static void shift_keypad_rows(void);
static BYTE is_key_pressed(void);
static BYTE convert_to_key(void);
static void store_detected_key(BYTE key);
static BYTE is_valid_led_number(BYTE key);
static void reset_internal_state(void);
static void set_column_active(BYTE col_index);
static void set_all_columns_inactive(void);
static BOOL is_row_pressed(BYTE row_bit);
static BYTE get_pressed_row(void);
static void print_detected_key(void); // For testing only

void KEY_Init(void)
{
    TRISA = 0xEA;
    ADCON1 = 0x0F;
    set_all_columns_inactive();
    reset_internal_state();
}

void KEY_Motor(void)
{
    switch (keypad_state)
    {
    case IDLE:
        shift_keypad_rows();
        if (is_key_pressed() && user_inside)
        {
            TiResetTics(TI_KEYPAD);
            keypad_state = ON_KEY_PRESS;
        }
        break;

    case ON_KEY_PRESS:
        if (TiGetTics(TI_KEYPAD) >= WAIT_16MS)
        {
            keypad_state = READ_KEY_VALUE;
        }
        break;

    case READ_KEY_VALUE:
        keypad_state = IDLE; // if key not pressed, go to IDLE
        if (is_key_pressed())
        {
            current_key = convert_to_key();
            keypad_state = CHECK_KEY_VALUE;
        }
        break;

    case CHECK_KEY_VALUE:
        print_detected_key();
        TiResetTics(TI_KEYPAD);
        keypad_state = STORE_KEY; // if key is not #, wait for release
        if (current_key == HASH_KEY)
        {
            waiting_for_second_key = FALSE;
            keypad_state = RESET_HOLD;
        }
        break;

    case STORE_KEY:
        store_detected_key(current_key);
        current_key = NO_KEY_PRESSED;
        keypad_state = ON_KEY_RELEASE;
        break;

    case ON_KEY_RELEASE:
        if (!is_key_pressed())
        {
            TiResetTics(TI_KEYPAD);
            keypad_state = WAIT_FOR_RELEASE;
        }
        break;

    case WAIT_FOR_RELEASE:
        if (TiGetTics(TI_KEYPAD) >= WAIT_16MS)
        {
            keypad_state = IDLE;
        }
        break;

    case RESET_HOLD:
        if (!is_key_pressed()) // if key stops being pressed, reset state
        {
            waiting_for_second_key = FALSE;
            keypad_state = IDLE;
        }
        else if (TiGetTics(TI_KEYPAD) >= WAIT_3S) // if key is pressed for 3 seconds, send reset command
        {
            command_ready = KEYPAD_RESET;
            waiting_for_second_key = FALSE;
            keypad_state = ON_KEY_RELEASE;
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
    command_ready = KEY_NO_COMMAND;
}

void KEY_SetUserInside(BOOL inside)
{
    if (!inside)
        reset_internal_state();
    user_inside = inside;
}

static void shift_keypad_rows(void)
{
    if (is_key_pressed())
        return; // If key is pressed, don't shift rows
    col_index = (col_index + 1) % KEYPAD_COLS;
    set_all_columns_inactive();
    set_column_active(col_index);
}

static BYTE is_key_pressed(void)
{
    return get_pressed_row() != NO_KEY_PRESSED;
}

static BYTE convert_to_key(void)
{
    BYTE row_index = get_pressed_row();
    return (row_index * 3) + col_index + 1;
}

static void store_detected_key(BYTE key)
{
    if (waiting_for_second_key)
    {
        led_intensity = key;
        command_ready = UPDATE_LED;
        waiting_for_second_key = FALSE;
        return;
    }
    if (is_valid_led_number(key))
    {
        led_number = key;
        waiting_for_second_key = TRUE;
    }
}

static BYTE is_valid_led_number(BYTE key)
{
    return (key <= MAX_LED_NUMBER);
}

static void reset_internal_state(void)
{
    keypad_state = IDLE;
    current_key = NO_KEY_PRESSED;
    col_index = 0;
    command_ready = KEY_NO_COMMAND;
    led_number = 0;
    led_intensity = 0;
    waiting_for_second_key = FALSE;
    user_inside = FALSE;
}

static void set_column_active(BYTE col_index)
{
    switch (col_index)
    {
    case COL0_INDEX:
        LATAbits.LATA2 = 1;
        break;
    case COL1_INDEX:
        LATAbits.LATA0 = 1;
        break;
    case COL2_INDEX:
        LATAbits.LATA4 = 1;
        break;
    }
}

static void set_all_columns_inactive(void)
{
    LATAbits.LATA2 = 0;
    LATAbits.LATA0 = 0;
    LATAbits.LATA4 = 0;
}

static BOOL is_row_pressed(BYTE row_bit)
{
    switch (row_bit)
    {
    case ROW0_PIN_BIT:
        return PORTAbits.RA1;
    case ROW1_PIN_BIT:
        return PORTAbits.RA6;
    case ROW2_PIN_BIT:
        return PORTAbits.RA5;
    case ROW3_PIN_BIT:
        return PORTAbits.RA3;
    default:
        return FALSE;
    }
}

static BYTE get_pressed_row(void)
{
    if (is_row_pressed(ROW0_PIN_BIT))
        return ROW0_INDEX;
    if (is_row_pressed(ROW1_PIN_BIT))
        return ROW1_INDEX;
    if (is_row_pressed(ROW2_PIN_BIT))
        return ROW2_INDEX;
    if (is_row_pressed(ROW3_PIN_BIT))
        return ROW3_INDEX;
    return NO_KEY_PRESSED;
}

// Method for testing only
static void print_detected_key(void)
{
    BYTE detected_char;
    if (current_key < 10)
    {
        detected_char = (current_key + 1) + '0';
    }
    else if (current_key == 10)
    {
        detected_char = '*';
    }
    else if (current_key == ZERO_KEY)
    {
        detected_char = '0';
    }
    else if (current_key == HASH_KEY)
    {
        detected_char = '#';
    }

    static BYTE buffer[11];
    // Initialize buffer on first use
    static BOOL buffer_initialized = FALSE;
    if (!buffer_initialized)
    {
        buffer[0] = '\r';
        buffer[1] = '\n';
        buffer[2] = 'K';
        buffer[3] = 'E';
        buffer[4] = 'Y';
        buffer[5] = ':';
        buffer[6] = ' ';
        buffer[7] = detected_char;
        buffer[8] = '\r';
        buffer[9] = '\n';
        buffer[10] = '\0';
        buffer_initialized = TRUE;
    }
    buffer[7] = detected_char;
    SIO_TEST_SendString(buffer);
}
