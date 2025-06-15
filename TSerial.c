#include "TSerial.h"

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

BOOL SIO_SendChar(BYTE character)
{
    if (!PIR1bits.TXIF)
        return FALSE;

    TXREG = character;
    return TRUE;
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

    // Send "Card detected!"
    send_string((BYTE *)msg_detected_card);

    // Send UID line
    send_string((BYTE *)msg_uid_prefix);
    send_string(UID);
    send_string((BYTE *)msg_crlf);

    // Send config line
    send_string((BYTE *)msg_config_prefix);
    SIO_SendChar(config[0] + '0');

    for (i = 1; i < 6; i++)
    {
        send_string((BYTE *)msg_separator);
        SIO_SendChar(i + '0');
        send_string((BYTE *)msg_colon);
        if (config[i] == 10)
            SIO_SendChar('A');
        else
            SIO_SendChar(config[i] + '0');
    }
    send_string((BYTE *)msg_crlf);
}

void SIO_SendMainMenu(void)
{
    send_string((BYTE *)msg_main_menu);
}

void SIO_SendUser(BYTE *User)
{
    send_string((BYTE *)msg_current_user);
    send_string(User);
    send_string((BYTE *)msg_crlf);
}

void SIO_SendNoUser(void)
{
    send_string((BYTE *)msg_no_user);
}

void SIO_SendStoredConfig(BYTE *UID, BYTE *config)
{
    BYTE i;

    send_string((BYTE *)msg_uid_prefix);
    send_string(UID);
    send_string((BYTE *)msg_arrow);
    send_string((BYTE *)msg_config_prefix);
    SIO_SendChar(config[0] + '0');

    for (i = 1; i < 6; i++)
    {
        send_string((BYTE *)msg_separator);
        SIO_SendChar(i + '0');
        send_string((BYTE *)msg_colon);
        if (config[i] == 10)
            SIO_SendChar('A');
        else
            SIO_SendChar(config[i] + '0');
    }
    send_string((BYTE *)msg_crlf);
}

void SIO_SendTimePrompt(void)
{
    send_string((BYTE *)msg_time_prompt);
}

void SIO_SendUnknownCard(BYTE *UID)
{
    send_string((BYTE *)msg_unknown_card);
    send_string(UID);
    send_string((BYTE *)msg_unknown_ignored);
}

/* =======================================
 *        PRIVATE FUNCTION BODIES
 * ======================================= */

static void send_string(BYTE *string)
{
    BYTE i = 0;
    while (string[i] != '\0')
    {
        while (!SIO_SendChar(string[i]))
            ;
        i++;
    }
}
