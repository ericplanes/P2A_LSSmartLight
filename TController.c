#include "TController.h"
#include "TSerial.h"
#include "TLCD.h"
#include "TKeypad.h"
#include "TLight.h"
#include "TRFID.h"
#include "TEEPROM.h"
#include "TUserControl.h"
#include "THora.h"
#include "TTimer.h"

/* =======================================
 *              CONSTANTS
 * ======================================= */

#define UID_SIZE 5
#define UID_STRING_SIZE 15
#define CONFIG_SIZE 6

// States
#define ST_INIT 0
#define ST_WAIT_INPUT 1
#define ST_PROCESS_KEYPAD 2
#define ST_PROCESS_RFID 3
#define ST_PROCESS_SERIAL 4
#define ST_UPDATE_TIME_WAIT 5
#define ST_UPDATE_STORED_CONFIG 6

/* =======================================
 *         PRIVATE VARIABLES
 * ======================================= */

static BYTE state = ST_INIT;
static BOOL user_inside;
static BYTE current_user_uid[UID_SIZE];
static BYTE current_user_position;
static BYTE current_config[CONFIG_SIZE];
static BYTE uid_buffer[UID_STRING_SIZE];
static BYTE time_hour = 0, time_minute = 0;

/* =======================================
 *       PRIVATE FUNCTION HEADERS
 * ======================================= */

static void format_uid(BYTE *uid);
static void reset_system(void);
static BYTE hex_char(BYTE val);

/* =======================================
 *         PUBLIC FUNCTION BODIES
 * ======================================= */

void CONTROLLER_Init(void)
{
    state = ST_INIT;
    user_inside = FALSE;
    current_user_position = USER_NOT_FOUND;

    for (BYTE i = 0; i < UID_SIZE; i++)
        current_user_uid[i] = 0;
    for (BYTE i = 0; i < CONFIG_SIZE; i++)
        current_config[i] = 0;

    KEY_SetUserInside(FALSE);
}

void CONTROLLER_Motor(void)
{
    BYTE cmd, led, intensity, pos;
    BYTE rfid_uid[UID_SIZE];

    switch (state)
    {
    case ST_INIT:
        SIO_SendMainMenu();
        LCD_WriteNoUserInfo();
        LED_UpdateConfig(current_config); // All zeroes
        state = ST_WAIT_INPUT;
        break;

    case ST_WAIT_INPUT:
        if (user_inside)
            state = ST_PROCESS_KEYPAD;
        else
            state = ST_PROCESS_RFID;
        break;

    case ST_PROCESS_KEYPAD:
        cmd = KEY_GetCommand();
        if (cmd == UPDATE_LED)
        {
            KEY_GetUpdateInfo(&led, &intensity);
            current_config[led] = intensity;
            KEY_ResetCommand();
            state = ST_UPDATE_STORED_CONFIG;
        }
        else if (cmd == KEYPAD_RESET)
        {
            reset_system();
            KEY_ResetCommand();
            state = ST_PROCESS_RFID;
        }

        break;
    case ST_UPDATE_STORED_CONFIG:
        if (EEPROM_StoreConfigForUser(current_user_position, current_config))
        {
            LED_UpdateConfig(current_config);
            LCD_WriteUserInfo(current_user_uid[4], current_config);
            state = ST_PROCESS_RFID;
        }
        break;
    case ST_PROCESS_RFID:
        if (RFID_HasReadUser() && RFID_GetReadUserId(rfid_uid))
        {
            pos = USER_FindPositionByRFID(rfid_uid);

            if (pos == USER_NOT_FOUND)
            {
                format_uid(rfid_uid);
                SIO_SendUnknownCard(uid_buffer);
            }
            else if (user_inside && pos == current_user_position)
            {
                // User leaving
                user_inside = FALSE;
                current_user_position = USER_NOT_FOUND;
                KEY_SetUserInside(FALSE);

                format_uid(current_user_uid);
                SIO_SendDetectedCard(uid_buffer, current_config);
                LCD_WriteNoUserInfo();

                for (BYTE i = 0; i < CONFIG_SIZE; i++)
                    current_config[i] = 0;
                LED_UpdateConfig(current_config);
            }
            else
            {
                // New user entering
                user_inside = TRUE;
                current_user_position = pos;
                for (BYTE i = 0; i < UID_SIZE; i++)
                    current_user_uid[i] = rfid_uid[i];

                EEPROM_ReadConfigForUser(pos, current_config);
                LED_UpdateConfig(current_config);

                format_uid(current_user_uid);
                SIO_SendDetectedCard(uid_buffer, current_config);
                LCD_WriteUserInfo(current_user_uid[4], current_config);
                KEY_SetUserInside(TRUE);
            }
        }

        state = ST_PROCESS_SERIAL;
        break;

    case ST_PROCESS_SERIAL:
        switch (SIO_ReadCommand())
        {
        case CMD_WHO_IN_ROOM:
            if (user_inside)
            {
                format_uid(current_user_uid);
                SIO_SendUser(uid_buffer);
            }
            else
            {
                SIO_SendNoUser();
            }
            break;

        case CMD_SHOW_STORED_CONF:
            for (pos = 0; pos < NUM_USERS; pos++)
            {
                const BYTE *stored_uid = USER_GetUserByPosition(pos);
                if (stored_uid)
                {
                    EEPROM_ReadConfigForUser(pos, current_config);
                    format_uid((BYTE *)stored_uid);
                    SIO_SendStoredConfig(uid_buffer, current_config);
                }
            }
            break;

        case CMD_UPDATE_TIME:
            SIO_SendTimePrompt();
            state = ST_UPDATE_TIME_WAIT;
            return;

        case CMD_ESC:
            SIO_SendMainMenu();
            break;

        default:
            break;
        }

        state = ST_WAIT_INPUT;
        break;

    case ST_UPDATE_TIME_WAIT:
        if (SIO_ReadTime(&time_hour, &time_minute))
        {
            HORA_SetTime(time_hour, time_minute);
            LCD_UpdateTime(time_hour, time_minute);
            state = ST_WAIT_INPUT;
        }
        break;
    }
}

/* =======================================
 *        PRIVATE FUNCTION BODIES
 * ======================================= */

static void format_uid(BYTE *uid)
{
    BYTE pos = 0;
    for (BYTE i = 0; i < UID_SIZE; i++)
    {
        uid_buffer[pos++] = hex_char((uid[i] >> 4) & 0x0F);
        uid_buffer[pos++] = hex_char(uid[i] & 0x0F);
        if (i < UID_SIZE - 1)
            uid_buffer[pos++] = '-';
    }
    uid_buffer[pos] = '\0';
}

static void reset_system(void)
{
    EEPROM_CleanMemory();
    user_inside = FALSE;
    current_user_position = USER_NOT_FOUND;

    for (BYTE i = 0; i < UID_SIZE; i++)
        current_user_uid[i] = 0;
    for (BYTE i = 0; i < CONFIG_SIZE; i++)
        current_config[i] = 0;

    LED_UpdateConfig(current_config);
    LCD_WriteNoUserInfo();
    KEY_SetUserInside(FALSE);
}

static BYTE hex_char(BYTE val)
{
    return (val < 10) ? ('0' + val) : ('A' + val - 10);
}
