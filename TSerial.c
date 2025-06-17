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
static const BYTE msg_main_menu[] = "---------------\r\nMain Menu\r\n---------------\r\nChoose:\r\n1.Who in room?\r\n2.Show configs\r\n3.Modify time\r\nOption: ";

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
            send_string((BYTE *)msg_crlf);

            // Reset state for next time
            state = TIME_STATE_HOUR_FIRST;
            return TRUE;
        }
        break;
    }

    return FALSE;
}

#ifdef DEBUG_MODE
void SIO_TEST_SendString(BYTE *string)
{
    send_string(string);
}
#endif

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

    send_string((BYTE *)msg_crlf);
    send_string((BYTE *)"Card detected!\r\nUID: ");
    send_string(UID);
    send_string((BYTE *)msg_crlf);
    send_string((BYTE *)"L0: ");
    send_char(config[0] + '0');

    for (i = 1; i < 6; i++)
    {
        send_string((BYTE *)" - L");
        send_char(i + '0');
        send_string((BYTE *)": ");
        send_char((config[i] == 10) ? 'A' : (config[i] + '0'));
    }
    send_string((BYTE *)msg_crlf);
}

void SIO_SendMainMenu(void)
{
    send_string((BYTE *)msg_crlf);
    send_string((BYTE *)msg_main_menu);
}

void SIO_SendUser(BYTE *User)
{
    send_string((BYTE *)msg_crlf);
    send_string((BYTE *)"Current user: UID ");
    send_string(User);
    send_string((BYTE *)msg_crlf);
}

void SIO_SendNoUser(void)
{
    send_string((BYTE *)msg_crlf);
    send_string((BYTE *)"No one in the room.\r\n");
}

void SIO_SendStoredConfig(BYTE *UID, BYTE *config)
{
    BYTE i;

    send_string((BYTE *)msg_crlf);
    send_string((BYTE *)"UID: ");
    send_string(UID);
    send_string((BYTE *)" -> L0: ");
    send_char(config[0] + '0');

    for (i = 1; i < 6; i++)
    {
        send_string((BYTE *)" - L");
        send_char(i + '0');
        send_string((BYTE *)": ");
        send_char((config[i] == 10) ? 'A' : (config[i] + '0'));
    }
    send_string((BYTE *)msg_crlf);
}

void SIO_SendTimePrompt(void)
{
    send_string((BYTE *)msg_crlf);
    send_string((BYTE *)"Enter new time (HH:MM): ");
}

void SIO_SendUnknownCard(BYTE *UID)
{
    send_string((BYTE *)msg_crlf);
    send_string((BYTE *)"Card detected!\r\nUnknown UID: ");
    send_string(UID);
    send_string((BYTE *)"\r\nCard not recognized. Ignored.\r\n");
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
