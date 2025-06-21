#include "TKeypad.h"
#include "TTimer.h"
#include "TSerial.h"

#define WAIT_16MS TWO_MS * 8
#define WAIT_3S ONE_SECOND * 3

// Special key defines
#define HASH_KEY 11
#define STAR_KEY 10
#define NO_KEY_PRESSED 12

// Pin assignments (PORTA)
#define COL0_PIN_BIT 2 // A2
#define COL1_PIN_BIT 0 // A0
#define COL2_PIN_BIT 4 // A4
#define ROW0_PIN_BIT 1 // A1
#define ROW1_PIN_BIT 6 // A6
#define ROW2_PIN_BIT 5 // A5
#define ROW3_PIN_BIT 3 // A3

#define TRISA_KEYPAD_CONFIG 0xEA
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
#define VALIDATE 3
#define ON_KEY_RELEASE 4
#define EXEC_KEY 5
#define RESET_HOLD 6

static BYTE keypad_state = IDLE;
static BYTE current_key = NO_KEY_PRESSED;
static BYTE col_index = 0;
static BYTE command_ready = NO_COMMAND;
static BYTE led_number = 0;
static BYTE led_intensity = 0;
static BOOL waiting_for_second_key = FALSE;
static BOOL user_inside = FALSE;
static BOOL scanning_paused = FALSE;

static void scan_keypad(void);
static BYTE is_key_pressed(void);
static BYTE convert_to_key(void);
static void process_detected_key(BYTE key);
static BYTE is_valid_led_number(BYTE key);
static BYTE is_hash_key(BYTE key);
static void reset_internal_state(void);
static void set_column_active(BYTE col_index);
static void set_all_columns_inactive(void);
static BOOL is_row_pressed(BYTE row_bit);
static BYTE get_pressed_row(void);

void KEY_Init(void)
{
    TRISA = TRISA_KEYPAD_CONFIG;
    set_all_columns_inactive();
    reset_internal_state();
}

void KEY_Motor(void)
{
    switch (keypad_state)
    {
    case IDLE:
        if (!scanning_paused)
            scan_keypad();
        if (is_key_pressed() && user_inside)
        {
            scanning_paused = TRUE;
            TiResetTics(TI_KEYPAD);
            keypad_state = ON_KEY_PRESS;
        }
        else if (scanning_paused && !is_key_pressed())
        {
            scanning_paused = FALSE;
        }
        break;

    case ON_KEY_PRESS:
        if (TiGetTics(TI_KEYPAD) >= WAIT_16MS)
            keypad_state = READ_KEY_VALUE;
        break;

    case READ_KEY_VALUE:
        keypad_state = is_key_pressed() ? VALIDATE : IDLE;
        if (keypad_state == VALIDATE)
            current_key = convert_to_key();
        break;

    case VALIDATE:
        if (current_key == NO_KEY_PRESSED)
        {
            keypad_state = IDLE;
        }
        else if (is_hash_key(current_key))
        {
            waiting_for_second_key = FALSE;
            keypad_state = RESET_HOLD;
        }
        else
        {
            TiResetTics(TI_KEYPAD);
            keypad_state = ON_KEY_RELEASE;
        }
        break;

    case ON_KEY_RELEASE:
        if (TiGetTics(TI_KEYPAD) >= WAIT_16MS)
            keypad_state = is_key_pressed() ? IDLE : EXEC_KEY;
        break;

    case EXEC_KEY:
        process_detected_key(current_key);
        current_key = NO_KEY_PRESSED;
        scanning_paused = FALSE;
        keypad_state = IDLE;
        break;

    case RESET_HOLD:
        if (!is_key_pressed())
        {
            waiting_for_second_key = FALSE;
            scanning_paused = FALSE;
            keypad_state = IDLE;
        }
        else if (TiGetTics(TI_KEYPAD) >= WAIT_3S)
        {
            command_ready = KEYPAD_RESET;
            waiting_for_second_key = FALSE;
            scanning_paused = FALSE;
            keypad_state = IDLE;
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
        reset_internal_state();
    user_inside = inside;
}

void KEY_ResetCommand(void)
{
    command_ready = NO_COMMAND;
}

static void scan_keypad(void)
{
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
    if (row_index == NO_KEY_PRESSED)
        return NO_KEY_PRESSED;
    if (row_index == 3)
    {
        if (col_index == 0)
            return STAR_KEY;
        if (col_index == 1)
            return 0;
        return HASH_KEY;
    }
    return (row_index * 3) + col_index + 1;
}

static void process_detected_key(BYTE key)
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

static BYTE is_hash_key(BYTE key)
{
    return (key == HASH_KEY);
}

static void reset_internal_state(void)
{
    keypad_state = IDLE;
    current_key = NO_KEY_PRESSED;
    col_index = 0;
    command_ready = NO_COMMAND;
    led_number = 0;
    led_intensity = 0;
    waiting_for_second_key = FALSE;
    scanning_paused = FALSE;
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
