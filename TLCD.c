#include "TLCD.h"
#include "TTimer.h"
#include "Utils.h"

/* =======================================
 *           HARDWARE CONFIGURATION
 * ======================================= */

// New LCD pin assignments:
// Control pins: RS->RD5, RW->RD6, E->RD7
// Data pins: D4->RB0, D5->RB1, D6->RB2, D7->RB3

// Data pins control macros (RB0-RB3, unchanged)
#define set_data_pins_output() (TRISBbits.TRISB0 = TRISBbits.TRISB1 = TRISBbits.TRISB2 = TRISBbits.TRISB3 = 0)
#define set_data_pins_input() (TRISBbits.TRISB0 = TRISBbits.TRISB1 = TRISBbits.TRISB2 = TRISBbits.TRISB3 = 1)

// Control pins configuration (updated to RD5, RD6, RD7)
#define set_control_pins_output() (TRISDbits.TRISD5 = 0, TRISDbits.TRISD6 = 0, TRISDbits.TRISD7 = 0)

// Individual data bit control (RB0-RB3, unchanged)
#define set_data_bit_4(state) (LATBbits.LATB0 = (state))
#define set_data_bit_5(state) (LATBbits.LATB1 = (state))
#define set_data_bit_6(state) (LATBbits.LATB2 = (state))
#define set_data_bit_7(state) (LATBbits.LATB3 = (state))

// Control pins (updated to use RD5, RD6, RD7)
#define get_busy_flag() (PORTBbits.RB3)                 // D7 data line for busy flag
#define set_register_select_high() (LATDbits.LATD5 = 1) // RS -> RD5
#define set_register_select_low() (LATDbits.LATD5 = 0)  // RS -> RD5
#define set_read_write_high() (LATDbits.LATD6 = 1)      // RW -> RD6
#define set_read_write_low() (LATDbits.LATD6 = 0)       // RW -> RD6
#define set_enable_high() (LATDbits.LATD7 = 1)          // E -> RD7 (unchanged)
#define set_enable_low() (LATDbits.LATD7 = 0)           // E -> RD7 (unchanged)

/* =======================================
 *           LCD COMMAND CONSTANTS
 * ======================================= */
// HD44780 Commands
#define LCD_CLEAR_DISPLAY 0x01
#define LCD_RETURN_HOME 0x02
#define LCD_ENTRY_MODE_SET 0x04
#define LCD_DISPLAY_CONTROL 0x08
#define LCD_CURSOR_SHIFT 0x10
#define LCD_FUNCTION_SET 0x20
#define LCD_SET_CGRAM_ADDR 0x40
#define LCD_SET_DDRAM_ADDR 0x80

// Entry Mode Set flags
#define LCD_ENTRY_INCREMENT 0x02
#define LCD_ENTRY_SHIFT 0x01

// Display Control flags
#define LCD_DISPLAY_ON 0x04
#define LCD_CURSOR_ON 0x02
#define LCD_BLINK_ON 0x01

// Function Set flags
#define LCD_8BIT_MODE 0x10
#define LCD_4BIT_MODE 0x00
#define LCD_2_LINE 0x08
#define LCD_1_LINE 0x00
#define LCD_5x10_DOTS 0x04
#define LCD_5x8_DOTS 0x00

/* =======================================
 *           PRIVATE VARIABLES
 * ======================================= */
static BYTE current_row;
static BYTE current_column;
static BYTE current_hour = 0;   // System hour (0-23)
static BYTE current_minute = 0; // System minute (0-59)

/* =======================================
 *        PRIVATE FUNCTION PROTOTYPES
 * ======================================= */
static void delay_ms(BYTE milliseconds);
static void send_instruction(BYTE instruction);
static void send_data(BYTE data);
static void wait_for_busy(void);
static void send_nibble(BYTE nibble);
static void send_nibble_init(BYTE nibble);
static void send_instruction_init(BYTE instruction);
static void set_cursor_position(BYTE row, BYTE column);
static void write_character(BYTE character);
static void write_string(const BYTE *string);
static BYTE hex_to_char(BYTE value);

/* =======================================
 *          PUBLIC FUNCTIONS
 * ======================================= */

void LCD_Init(void)
{
    // Initialize variables
    current_row = 0;
    current_column = 0;

    // Configure hardware pins
    // Control pins: RS->RD5, RW->RD6, E->RD7
    // Data pins: D4->RB0, D5->RB1, D6->RB2, D7->RB3
    set_control_pins_output();
    set_data_pins_output();

    // Initialize control pins
    set_register_select_low();
    set_read_write_low();
    set_enable_low();

    // Robust HD44780 initialization sequence with double execution
    // This approach increases reliability with non-compliant displays
    for (BYTE init_attempt = 0; init_attempt < 2; init_attempt++)
    {
        // Extended power stabilization wait (150ms for maximum compatibility)
        delay_ms(75); // 75ms * 2 = 150ms actual wait time

        // HD44780 8-bit startup sequence (NO busy flag checking yet)
        // Send 0x3 three times as per HD44780 datasheet
        send_nibble_init(0x3);
        delay_ms(10); // 20ms actual - well above 4.1ms minimum

        send_nibble_init(0x3);
        delay_ms(2); // 4ms actual - well above 100μs minimum

        send_nibble_init(0x3);
        delay_ms(2); // 4ms actual - well above 100μs minimum

        // Switch to 4-bit mode with 0x2
        send_nibble_init(0x2);
        delay_ms(2); // 4ms actual - safe margin

        // Configure LCD in 4-bit mode (still NO busy flag checking)
        send_instruction_init(LCD_FUNCTION_SET | LCD_4BIT_MODE | LCD_2_LINE | LCD_5x8_DOTS);
        delay_ms(1); // 2ms actual

        send_instruction_init(LCD_DISPLAY_CONTROL); // Display off
        delay_ms(1);                                // 2ms actual

        send_instruction_init(LCD_CLEAR_DISPLAY); // Clear all DDRAM
        delay_ms(6);                              // 12ms actual - well above 1.52ms clear time

        send_instruction_init(LCD_ENTRY_MODE_SET | LCD_ENTRY_INCREMENT);
        delay_ms(1); // 2ms actual

        send_instruction_init(LCD_DISPLAY_CONTROL | LCD_DISPLAY_ON); // Display on
        delay_ms(1);                                                 // 2ms actual
    }

    // After double initialization, LCD is fully ready for busy flag usage
}

void LCD_WriteNoUserInfo(void)
{
    // Clear display
    send_instruction(LCD_CLEAR_DISPLAY);
    delay_ms(2);

    // Set cursor to home position
    set_cursor_position(0, 0);

    // Display default state with current system time: "- HH:MM 1-0 2-0 3-0 4-0 5-0 6-0"
    write_character('-');
    write_character(' ');

    // Write current system time
    write_character((current_hour / 10) + '0');
    write_character((current_hour % 10) + '0');
    write_character(':');
    write_character((current_minute / 10) + '0');
    write_character((current_minute % 10) + '0');

    // Write light configuration
    write_string((const BYTE *)" 1-0 2-0");
    set_cursor_position(1, 0);
    write_string((const BYTE *)"3-0 4-0 5-0 6-0");
}

void LCD_WriteUserInfo(BYTE last_uid_char, BYTE *light_config)
{
    BYTE i;

    // Clear display
    send_instruction(LCD_CLEAR_DISPLAY);
    delay_ms(2);

    // Set cursor to home position
    set_cursor_position(0, 0);

    // Write user character
    write_character(last_uid_char);
    write_character(' ');

    // Write current system time
    write_character((current_hour / 10) + '0');
    write_character((current_hour % 10) + '0');
    write_character(':');
    write_character((current_minute / 10) + '0');
    write_character((current_minute % 10) + '0');

    // Update light configuration
    LCD_UpdateLightConfig(light_config);
}

void LCD_UpdateTime(BYTE hour, BYTE minute)
{
    // Store current time in static variables
    current_hour = hour;
    current_minute = minute;

    // Position cursor at time location (row 0, column 2)
    set_cursor_position(0, 2);

    // Update time display
    write_character((hour / 10) + '0');
    write_character((hour % 10) + '0');
    write_character(':');
    write_character((minute / 10) + '0');
    write_character((minute % 10) + '0');
}

void LCD_UpdateLightConfig(BYTE *light_config)
{
    // Light 1 and 2 on first line
    set_cursor_position(0, 10); // "1-X" - position of X
    write_character(hex_to_char(light_config[0]));

    set_cursor_position(0, 14); // "2-Y" - position of Y
    write_character(hex_to_char(light_config[1]));

    // Lights 3, 4, 5, 6 on second line
    set_cursor_position(1, 2); // "3-Z" - position of Z
    write_character(hex_to_char(light_config[2]));

    set_cursor_position(1, 6); // "4-W" - position of W
    write_character(hex_to_char(light_config[3]));

    set_cursor_position(1, 10); // "5-V" - position of V
    write_character(hex_to_char(light_config[4]));

    set_cursor_position(1, 14); // "6-U" - position of U
    write_character(hex_to_char(light_config[5]));
}

/* =======================================
 *          PRIVATE FUNCTIONS
 * ======================================= */

// NOTE: Two types of timing are used in this module:
// 1. Long delays (ms): For initialization sequence and clear command (using TTimer)
// 2. Enable pulse timing: Double function calls ensure sufficient pulse width

static void delay_ms(BYTE milliseconds)
{
    WORD target_ticks = (WORD)milliseconds / (WORD)(2 / TWO_MS); // Each tick is 2ms
    if (target_ticks == 0)
        target_ticks = 1; // Minimum 1 tick for delays < 2ms

    TiResetTics(TI_LCD);
    while (TiGetTics(TI_LCD) < target_ticks)
        ;
}

static void send_nibble(BYTE nibble)
{
    // Set data pins
    set_data_bit_7(nibble & 0x08 ? 1 : 0);
    set_data_bit_6(nibble & 0x04 ? 1 : 0);
    set_data_bit_5(nibble & 0x02 ? 1 : 0);
    set_data_bit_4(nibble & 0x01 ? 1 : 0);

    // Pulse enable pin (double call ensures sufficient pulse width)
    set_enable_high();
    set_enable_high(); // Making sure the pulse lasts enough time
    set_enable_low();
    set_enable_low();
}

static void send_nibble_init(BYTE nibble)
{
    // Initialization nibble send (8-bit mode emulation during startup)
    // NO busy flag checking - uses only timed delays
    set_data_pins_output();
    set_register_select_low(); // Instruction mode
    set_read_write_low();      // Write mode

    // Set data pins for 8-bit startup command
    set_data_bit_7(nibble & 0x08 ? 1 : 0);
    set_data_bit_6(nibble & 0x04 ? 1 : 0);
    set_data_bit_5(nibble & 0x02 ? 1 : 0);
    set_data_bit_4(nibble & 0x01 ? 1 : 0);

    // Pulse enable pin
    set_enable_high();
    set_enable_high(); // Ensure sufficient pulse width
    set_enable_low();
    set_enable_low();
}

static void send_instruction_init(BYTE instruction)
{
    // Send 4-bit instruction during initialization (NO busy flag checking)
    set_data_pins_output();
    set_register_select_low(); // Instruction mode
    set_read_write_low();      // Write mode

    // Send upper nibble
    set_data_bit_7(instruction & 0x80 ? 1 : 0);
    set_data_bit_6(instruction & 0x40 ? 1 : 0);
    set_data_bit_5(instruction & 0x20 ? 1 : 0);
    set_data_bit_4(instruction & 0x10 ? 1 : 0);

    set_enable_high();
    set_enable_high();
    set_enable_low();
    set_enable_low();

    // Send lower nibble
    set_data_bit_7(instruction & 0x08 ? 1 : 0);
    set_data_bit_6(instruction & 0x04 ? 1 : 0);
    set_data_bit_5(instruction & 0x02 ? 1 : 0);
    set_data_bit_4(instruction & 0x01 ? 1 : 0);

    set_enable_high();
    set_enable_high();
    set_enable_low();
    set_enable_low();
}

static void send_instruction(BYTE instruction)
{
    wait_for_busy();

    set_data_pins_output();
    set_register_select_low(); // Instruction mode
    set_read_write_low();      // Write mode

    // Send upper nibble
    send_nibble(instruction >> 4);

    // Send lower nibble
    send_nibble(instruction & 0x0F);
}

static void send_data(BYTE data)
{
    wait_for_busy();

    set_data_pins_output();
    set_register_select_high(); // Data mode
    set_read_write_low();       // Write mode

    // Send upper nibble
    send_nibble(data >> 4);

    // Send lower nibble
    send_nibble(data & 0x0F);
}

static void wait_for_busy(void)
{
    BOOL busy_flag;
    WORD timeout_ticks;

    set_data_pins_input();
    set_register_select_low(); // Read instruction register
    set_read_write_high();     // Read mode

    TiResetTics(TI_LCD);
    do
    {
        // Read busy flag (upper nibble)
        set_enable_high();
        set_enable_high();           // Making sure the pulse lasts enough time
        busy_flag = get_busy_flag(); // Busy flag is bit 7 (RB3 in our case)
        set_enable_low();
        set_enable_low();

        // Read lower nibble (address counter - not used but required)
        set_enable_high();
        set_enable_high();
        set_enable_low();
        set_enable_low();

        // Timeout protection (more than 1ms means LCD has gone mad)
        timeout_ticks = TiGetTics(TI_LCD);
        if (timeout_ticks > 0) // 0 ticks = < 2ms, 1 tick = >= 2ms
            break;

    } while (busy_flag);

    set_read_write_low(); // Return to write mode
}

static void set_cursor_position(BYTE row, BYTE column)
{
    BYTE address;

    if (row == 0)
    {
        address = 0x00 + column; // First line starts at 0x00
    }
    else
    {
        address = 0x40 + column; // Second line starts at 0x40
    }

    send_instruction(LCD_SET_DDRAM_ADDR | address);

    current_row = row;
    current_column = column;
}

static void write_character(BYTE character)
{
    send_data(character);
    current_column++;

    // Handle line wrapping
    if (current_column >= 16)
    {
        current_column = 0;
        current_row = (current_row + 1) % 2;
        set_cursor_position(current_row, current_column);
    }
}

static void write_string(const BYTE *string)
{
    while (*string)
    {
        write_character(*string);
        string++;
    }
}

static BYTE hex_to_char(BYTE value)
{
    if (value <= 9)
    {
        return '0' + value;
    }
    else if (value == 10)
    {
        return 'A';
    }
    return '0'; // Default for invalid values
}
