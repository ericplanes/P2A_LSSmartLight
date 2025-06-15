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

// Static strings for messages
static const BYTE msg_detected_card[] = "Card detected!\r\n";
static const BYTE msg_uid_prefix[] = "\tUID: ";
static const BYTE msg_config_prefix[] = "\tL0: ";
static const BYTE msg_separator[] = " - L";
static const BYTE msg_colon[] = ": ";
static const BYTE msg_main_menu[] = "---------------\r\n"
                                    "\tMain Menu\r\n"
                                    "---------------\r\n"
                                    "Choose an option:\r\n"
                                    "\t1. Who is in the room?\r\n"
                                    "\t2. Show configurations\r\n"
                                    "\t3. Modify system time\r\n"
                                    "Option: ";
static const BYTE msg_current_user[] = "Current user: UID ";
static const BYTE msg_no_user[] = "No one in the room.\r\n";
static const BYTE msg_time_prompt[] = "Enter new time (HH:MM): ";
static const BYTE msg_unknown_card[] = "Card detected!\r\nUnknown UID: ";
static const BYTE msg_unknown_ignored[] = "\r\nCard not recognized. Ignored.\r\n";
static const BYTE msg_arrow[] = " -> ";
static const BYTE msg_crlf[] = "\r\n";

/* =======================================
 *        PRIVATE FUNCTION HEADERS
 * ======================================= */

static BOOL send_char(BYTE character);
static void send_string(BYTE *string);

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
}

BOOL SIO_ReadTime(BYTE *hour, BYTE *mins)
{
    static BYTE state = TIME_STATE_HOUR_FIRST;
    static BYTE hour_chars[2], min_chars[2];

    if (!PIR1bits.RC1IF)
        return FALSE;

    BYTE received_char = RCREG;

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
            // Send confirmation "\r\n"
            send_string((BYTE *)msg_crlf);

            // Reset state for next time
            state = TIME_STATE_HOUR_FIRST;
            return TRUE;
        }
        break;
    }

    return FALSE;
}

void SIO_TEST_SendString(BYTE *string)
{
    send_string(string);
}

BYTE SIO_ReadCommand(void)
{
    if (!PIR1bits.RC1IF)
        return CMD_NO_COMMAND;

    BYTE ascii = RCREG;

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

void SIO_SendDetectedCard(BYTE *UID, BYTE *config)
{
    BYTE i;

    // Ensure new line
    send_string((BYTE *)msg_crlf);

    // Send "Card detected!"
    send_string((BYTE *)msg_detected_card);

    // Send UID line
    send_string((BYTE *)msg_uid_prefix);
    send_string(UID);
    send_string((BYTE *)msg_crlf);

    // Send config line
    send_string((BYTE *)msg_config_prefix);
    send_char(config[0] + '0');

    for (i = 1; i < 6; i++)
    {
        send_string((BYTE *)msg_separator);
        send_char(i + '0');
        send_string((BYTE *)msg_colon);
        if (config[i] == 10)
            send_char('A');
        else
            send_char(config[i] + '0');
    }
    send_string((BYTE *)msg_crlf);
}

void SIO_SendMainMenu(void)
{
    // Ensure new line
    send_string((BYTE *)msg_crlf);

    send_string((BYTE *)msg_main_menu);
}

void SIO_SendUser(BYTE *User)
{
    // Ensure new line
    send_string((BYTE *)msg_crlf);

    send_string((BYTE *)msg_current_user);
    send_string(User);
    send_string((BYTE *)msg_crlf);
}

void SIO_SendNoUser(void)
{
    // Ensure new line
    send_string((BYTE *)msg_crlf);

    send_string((BYTE *)msg_no_user);
}

void SIO_SendStoredConfig(BYTE *UID, BYTE *config)
{
    BYTE i;

    // Ensure new line
    send_string((BYTE *)msg_crlf);

    send_string((BYTE *)msg_uid_prefix);
    send_string(UID);
    send_string((BYTE *)msg_arrow);
    send_string((BYTE *)msg_config_prefix);
    send_char(config[0] + '0');

    for (i = 1; i < 6; i++)
    {
        send_string((BYTE *)msg_separator);
        send_char(i + '0');
        send_string((BYTE *)msg_colon);
        if (config[i] == 10)
            send_char('A');
        else
            send_char(config[i] + '0');
    }
    send_string((BYTE *)msg_crlf);
}

void SIO_SendTimePrompt(void)
{
    // Ensure new line
    send_string((BYTE *)msg_crlf);

    send_string((BYTE *)msg_time_prompt);
}

void SIO_SendUnknownCard(BYTE *UID)
{
    // Ensure new line
    send_string((BYTE *)msg_crlf);

    send_string((BYTE *)msg_unknown_card);
    send_string(UID);
    send_string((BYTE *)msg_unknown_ignored);
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
