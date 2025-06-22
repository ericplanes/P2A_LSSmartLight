#include "TSerial.h"

/* =======================================
 *         PRIVATE CONSTANTS
 * ======================================= */

// SIO_ReadTime state machine states
#define TIME_STATE_HOUR_FIRST 0
#define TIME_STATE_HOUR_SECOND 1
#define TIME_STATE_MIN_FIRST 2
#define TIME_STATE_MIN_SECOND 3

/* =======================================
 *         PRIVATE VARIABLES
 * ======================================= */

// Optimized string constants (reduced memory usage)
static const BYTE msg_crlf[] = "\r\n";
static const BYTE msg_main_menu[] = "---------------\r\n    Main Menu\r\n---------------\r\nChoose:\r\n    1.Who in room?\r\n    2.Show configs\r\n    3.Modify time\r\nOption: ";

// Buffer for UID formatting
static BYTE uid_buffer[15]; // 5 bytes * 2 chars + 4 dashes + null terminator

// Buffer for config formatting
static BYTE config_buffer[40]; // "L0: 0 - L1: 3 - L2: 9 - L3: A - L4: 0 - L5: 9" + null terminator

/* =======================================
 *        PRIVATE FUNCTION HEADERS
 * ======================================= */

static BOOL send_char(BYTE character);
static void send_string(BYTE *string);
static void clear_before_new_message(void);
static void format_uid(const BYTE *uid);
static void format_config(const BYTE *config);
static BYTE hex_char(BYTE val);
static void init_uid_buffer(void);

/* =======================================
 *         PUBLIC FUNCTION BODIES
 * ======================================= */

void SIO_Init(void)
{
    TRISCbits.TRISC6 = 0; // TX output
    TRISCbits.TRISC7 = 1; // RX input

    TXSTAbits.BRGH = 1;
    BAUDCONbits.BRG16 = 0;
    SPBRG = 207; // 9600 baud @ 32 MHz

    TXSTAbits.SYNC = 0;
    TXSTAbits.TXEN = 1;
    RCSTAbits.SPEN = 1;
    RCSTAbits.CREN = 1;
    init_uid_buffer();
}

BOOL SIO_ReadTime(BYTE *hour, BYTE *mins)
{
    static BYTE state = TIME_STATE_HOUR_FIRST;
    static BYTE hour_chars[2], min_chars[2];

    if (!PIR1bits.RC1IF)
        return FALSE;

    BYTE received_char = RCREG;
    send_char(received_char);

    switch (state)
    {
    case TIME_STATE_HOUR_FIRST: // First hour digit
        if (received_char >= '0' && received_char <= '9')
        {
            hour_chars[0] = received_char;
            state = TIME_STATE_HOUR_SECOND;
        }
        break;

    case TIME_STATE_HOUR_SECOND: // Second hour digit
        if (received_char >= '0' && received_char <= '9')
        {
            hour_chars[1] = received_char;
            *hour = (hour_chars[0] - '0') * 10 + (hour_chars[1] - '0');
            while (!send_char(':'))
                ;
            state = TIME_STATE_MIN_FIRST;
        }
        break;

    case TIME_STATE_MIN_FIRST: // First minute digit
        if (received_char >= '0' && received_char <= '9')
        {
            min_chars[0] = received_char;
            state = TIME_STATE_MIN_SECOND;
        }
        break;

    case TIME_STATE_MIN_SECOND: // Second minute digit
        if (received_char >= '0' && received_char <= '9')
        {
            min_chars[1] = received_char;
            *mins = (min_chars[0] - '0') * 10 + (min_chars[1] - '0');
            send_string((BYTE *)msg_crlf);

            // Reset state for next time
            state = TIME_STATE_HOUR_FIRST;
            clear_before_new_message();
            send_string((BYTE *)"Time updated successfully.\r\n");
            return TRUE;
        }
        break;
    }

    return FALSE;
}

BYTE SIO_ReadCommand(void)
{
    if (!PIR1bits.RC1IF)
        return CMD_NO_COMMAND;

    BYTE ascii = RCREG;
    send_char(ascii);

    // Check for valid commands using switch
    switch (ascii)
    {
    case ASCII_1:
        return CMD_WHO_IN_ROOM;
    case ASCII_2:
        return CMD_SHOW_STORED_CONF;
    case ASCII_3:
        return CMD_UPDATE_TIME;
    case ASCII_ESC:
        return CMD_ESC;
    default:
        return CMD_NO_COMMAND;
    }
}

void SIO_SendDetectedCard(const BYTE *uid_bytes, const BYTE *config)
{
    format_uid(uid_bytes);
    format_config(config);

    clear_before_new_message();
    send_string((BYTE *)"Card detected!\r\nUID: ");
    send_string(uid_buffer);
    send_string((BYTE *)msg_crlf);
    send_string(config_buffer);
    send_string((BYTE *)msg_crlf);
}

void SIO_SendMainMenu(void)
{
    clear_before_new_message();
    send_string((BYTE *)msg_main_menu);
}

void SIO_SendUser(const BYTE *uid_bytes)
{
    format_uid(uid_bytes);

    send_string((BYTE *)msg_crlf);
    send_string((BYTE *)"Current user: UID ");
    send_string(uid_buffer);
    send_string((BYTE *)msg_crlf);
}

void SIO_SendNoUser(void)
{
    clear_before_new_message();
    send_string((BYTE *)"No one in the room.\r\n");
}

void SIO_SendStoredConfig(const BYTE *uid_bytes, const BYTE *config)
{
    format_uid(uid_bytes);
    format_config(config);

    clear_before_new_message();
    send_string((BYTE *)"UID: ");
    send_string(uid_buffer);
    send_string((BYTE *)" -> ");
    send_string(config_buffer);
    send_string((BYTE *)msg_crlf);
}

void SIO_SendTimePrompt(void)
{
    clear_before_new_message();
    send_string((BYTE *)"Enter new time (HH:MM): ");
}

void SIO_SendUnknownCard(const BYTE *uid_bytes)
{
    format_uid(uid_bytes);

    clear_before_new_message();
    send_string((BYTE *)"Card detected!\r\nUnknown UID: ");
    send_string(uid_buffer);
    send_string((BYTE *)"\r\nCard not recognized. Ignored.\r\n");
}

void SIO_SendKeyReset(void)
{
    send_string((BYTE *)"\r\nKeypad RESET Triggered! Cleaning up...");
    SIO_SendMainMenu();
}

/* =======================================
 *        PRIVATE FUNCTION BODIES
 * ======================================= */

static BOOL send_char(BYTE character)
{
    if (!PIR1bits.TXIF)
        return FALSE;

    TXREG = character;
    return TRUE;
}

static void send_string(BYTE *string)
{
    BYTE i = 0;
    while (string[i] != '\0')
    {
        while (!send_char(string[i]))
            ;
        i++;
    }
}

static void clear_before_new_message(void)
{
    send_string((BYTE *)msg_crlf);
    send_string((BYTE *)msg_crlf);
}

static void format_uid(const BYTE *uid)
{
    uid_buffer[0] = hex_char((uid[0] >> 4) & 0x0F);
    uid_buffer[1] = hex_char(uid[0] & 0x0F);

    uid_buffer[3] = hex_char((uid[1] >> 4) & 0x0F);
    uid_buffer[4] = hex_char(uid[1] & 0x0F);

    uid_buffer[6] = hex_char((uid[2] >> 4) & 0x0F);
    uid_buffer[7] = hex_char(uid[2] & 0x0F);

    uid_buffer[9] = hex_char((uid[3] >> 4) & 0x0F);
    uid_buffer[10] = hex_char((uid[3] & 0x0F));

    uid_buffer[12] = hex_char((uid[4] >> 4) & 0x0F);
    uid_buffer[13] = hex_char(uid[4] & 0x0F);
}

static void format_config(const BYTE *config)
{
    BYTE pos = 0;
    for (BYTE i = 0; i < 6; i++) // 6 LEDs
    {
        if (i > 0)
        {
            config_buffer[pos++] = ' ';
            config_buffer[pos++] = '-';
            config_buffer[pos++] = ' ';
        }

        config_buffer[pos++] = 'L';
        config_buffer[pos++] = i + '0';
        config_buffer[pos++] = ':';
        config_buffer[pos++] = ' ';
        config_buffer[pos++] = hex_char(config[i]);
    }
    config_buffer[pos] = '\0';
}

static BYTE hex_char(BYTE val)
{
    if (val < 10)
        return '0' + val;
    return 'A' + val - 10;
}

static void init_uid_buffer(void)
{
    uid_buffer[0] = 'A';
    uid_buffer[1] = 'A';
    uid_buffer[2] = '-';
    uid_buffer[3] = 'B';
    uid_buffer[4] = 'B';
    uid_buffer[5] = '-';
    uid_buffer[6] = 'C';
    uid_buffer[7] = 'C';
    uid_buffer[8] = '-';
    uid_buffer[9] = 'D';
    uid_buffer[10] = 'D';
    uid_buffer[11] = '-';
    uid_buffer[12] = 'E';
    uid_buffer[13] = 'E';
    uid_buffer[14] = '\0';
}
