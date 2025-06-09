#include "TLCD.h"

// LCD Pin Definitions (adjust according to your hardware connections)
#define LCD_RS PORTDbits.RD0
#define LCD_EN PORTDbits.RD1
#define LCD_D4 PORTDbits.RD4
#define LCD_D5 PORTDbits.RD5
#define LCD_D6 PORTDbits.RD6
#define LCD_D7 PORTDbits.RD7

#define LCD_RS_TRIS TRISDbits.TRISD0
#define LCD_EN_TRIS TRISDbits.TRISD1
#define LCD_D4_TRIS TRISDbits.TRISD4
#define LCD_D5_TRIS TRISDbits.TRISD5
#define LCD_D6_TRIS TRISDbits.TRISD6
#define LCD_D7_TRIS TRISDbits.TRISD7

// LCD Commands
#define LCD_CLEAR 0x01
#define LCD_HOME 0x02
#define LCD_ENTRY_MODE 0x06
#define LCD_DISPLAY_ON 0x0C
#define LCD_FUNCTION_SET 0x28
#define LCD_SET_CURSOR 0x80

/* =======================================
 *       PRIVATE FUNCTION HEADERS
 * ======================================= */
static void pulse(void);
static void write_nibble(BYTE data);
static void write_command(BYTE cmd);
static void write_data(BYTE data);
static void write_string(const char *str);
static void set_cursor(BYTE row, BYTE col);
static char hex_to_char(BYTE value);

/* =======================================
 *          PUBLIC FUNCTION BODIES
 * ======================================= */

void LCD_Init(void)
{
    // Configure pins as outputs
    LCD_RS_TRIS = 0;
    LCD_EN_TRIS = 0;
    LCD_D4_TRIS = 0;
    LCD_D5_TRIS = 0;
    LCD_D6_TRIS = 0;
    LCD_D7_TRIS = 0;

    // Initialize pins to 0
    LCD_RS = 0;
    LCD_EN = 0;
    LCD_D4 = 0;
    LCD_D5 = 0;
    LCD_D6 = 0;
    LCD_D7 = 0;

    // Wait for LCD to power up
    __delay_ms(20);

    // Initialize LCD in 4-bit mode
    write_nibble(0x03);
    __delay_ms(5);
    write_nibble(0x03);
    __delay_us(150);
    write_nibble(0x03);
    __delay_us(150);
    write_nibble(0x02);
    __delay_us(150);

    // Configure LCD
    write_command(LCD_FUNCTION_SET);
    write_command(LCD_DISPLAY_ON);
    write_command(LCD_CLEAR);
    write_command(LCD_ENTRY_MODE);

    __delay_ms(2);
}

void LCD_Reset(void)
{
    write_command(LCD_CLEAR);
    set_cursor(0, 0);
    write_string("- 00:00 1-0 2-0");
    set_cursor(1, 0);
    write_string("3-0 4-0 5-0 6-0");
}

void LCD_WriteUserInfo(BYTE last_uid_char, BYTE hour, BYTE minute, BYTE *light_config)
{
    char buffer[17]; // 16 chars + null terminator

    write_command(LCD_CLEAR);

    // First line: "F 16:30 1-0 2-3"
    buffer[0] = hex_to_char(last_uid_char);
    buffer[1] = ' ';
    buffer[2] = '0' + (hour / 10);
    buffer[3] = '0' + (hour % 10);
    buffer[4] = ':';
    buffer[5] = '0' + (minute / 10);
    buffer[6] = '0' + (minute % 10);
    buffer[7] = ' ';
    buffer[8] = '1';
    buffer[9] = '-';
    buffer[10] = hex_to_char(light_config[0]);
    buffer[11] = ' ';
    buffer[12] = '2';
    buffer[13] = '-';
    buffer[14] = hex_to_char(light_config[1]);
    buffer[15] = ' ';
    buffer[16] = '\0';

    set_cursor(0, 0);
    write_string(buffer);

    // Second line: "3-3 4-0 5-9 6-A"
    buffer[0] = '3';
    buffer[1] = '-';
    buffer[2] = hex_to_char(light_config[2]);
    buffer[3] = ' ';
    buffer[4] = '4';
    buffer[5] = '-';
    buffer[6] = hex_to_char(light_config[3]);
    buffer[7] = ' ';
    buffer[8] = '5';
    buffer[9] = '-';
    buffer[10] = hex_to_char(light_config[4]);
    buffer[11] = ' ';
    buffer[12] = '6';
    buffer[13] = '-';
    buffer[14] = hex_to_char(light_config[5]);
    buffer[15] = '\0';

    set_cursor(1, 0);
    write_string(buffer);
}

void LCD_UpdateTime(BYTE hour, BYTE minute)
{
    set_cursor(0, 2);
    write_data('0' + (hour / 10));
    write_data('0' + (hour % 10));
    write_data(':');
    write_data('0' + (minute / 10));
    write_data('0' + (minute % 10));
}

void LCD_UpdateLightConfig(BYTE *light_config)
{
    // Update light 1 (row 0, col 10)
    set_cursor(0, 10);
    write_data(hex_to_char(light_config[0]));

    // Update light 2 (row 0, col 14)
    set_cursor(0, 14);
    write_data(hex_to_char(light_config[1]));

    // Update light 3 (row 1, col 2)
    set_cursor(1, 2);
    write_data(hex_to_char(light_config[2]));

    // Update light 4 (row 1, col 6)
    set_cursor(1, 6);
    write_data(hex_to_char(light_config[3]));

    // Update light 5 (row 1, col 10)
    set_cursor(1, 10);
    write_data(hex_to_char(light_config[4]));

    // Update light 6 (row 1, col 14)
    set_cursor(1, 14);
    write_data(hex_to_char(light_config[5]));
}

/* =======================================
 *          PRIVATE FUNCTION BODIES
 * ======================================= */

static void pulse(void)
{
    LCD_EN = 1;
    __delay_us(1);
    LCD_EN = 0;
    __delay_us(50);
}

static void write_nibble(BYTE data)
{
    LCD_D4 = (data >> 0) & 1;
    LCD_D5 = (data >> 1) & 1;
    LCD_D6 = (data >> 2) & 1;
    LCD_D7 = (data >> 3) & 1;
    pulse();
}

static void write_command(BYTE cmd)
{
    LCD_RS = 0;
    write_nibble(cmd >> 4);
    write_nibble(cmd & 0x0F);
    __delay_ms(2);
}

static void write_data(BYTE data)
{
    LCD_RS = 1;
    write_nibble(data >> 4);
    write_nibble(data & 0x0F);
    __delay_us(50);
}

static void write_string(const char *str)
{
    while (*str)
    {
        write_data(*str++);
    }
}

static void set_cursor(BYTE row, BYTE col)
{
    BYTE address = (row == 0) ? 0x80 + col : 0xC0 + col;
    write_command(address);
}

static char hex_to_char(BYTE value)
{
    if (value <= 9)
        return '0' + value;
    else if (value == 0x0A)
        return 'A';
    else
        return '0'; // Default for invalid values
}
