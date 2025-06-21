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
#define CONFIG_SIZE 6

/* =======================================
 *         CONTROLLER STATES
 * ======================================= */

#define BOOT_INIT_SYSTEM 0          // On system boot - initialize display and LEDs
#define INPUT_WAIT_DETECT 1         // Waits until input detected (keypad/RFID/serial)
#define KEY_PROCESS_CMD 2           // On keypad input - process LED update or reset
#define KEY_STORE_CONFIG 3          // After LED update - store to EEPROM
#define RFID_READ_CARD_DATA 4       // On card detected - read UID data
#define RFID_VALIDATE_USER 5        // On UID complete - validate against known users
#define RFID_USER_ENTER 6           // On valid new user - configure room
#define RFID_USER_EXIT 7            // On same user card - user leaving
#define RFID_LOAD_USER_CONFIG 8     // After user validation - load their config
#define SERIAL_PROCESS_CMD 9        // On serial input - process menu commands
#define SERIAL_SEND_WHO_RESPONSE 10 // On "who in room" - send current user
#define SERIAL_SEND_CONFIGS 11      // On "show configs" - send all stored configs
#define SERIAL_WAIT_TIME_INPUT 12   // After time request - wait for time data

/* =======================================
 *         PRIVATE VARIABLES
 * ======================================= */

static BYTE state = BOOT_INIT_SYSTEM;
static BOOL user_inside;
static BYTE current_user_uid[UID_SIZE];
static BYTE current_user_position;
static BYTE current_config[CONFIG_SIZE];
static BYTE time_hour = 0, time_minute = 0;

// Processing variables
static BYTE rfid_uid[UID_SIZE];
static BYTE cmd_buffer;
static BYTE led_num, led_intensity;
static BYTE user_pos;
static BYTE users_sent;

/* =======================================
 *       PRIVATE FUNCTION HEADERS
 * ======================================= */

static void reset_system(void);
static void clean_config(void);
static BYTE get_last_uid_char(const BYTE *uid);

/* =======================================
 *         PUBLIC FUNCTION BODIES
 * ======================================= */

void CONTROLLER_Init(void)
{
    state = BOOT_INIT_SYSTEM;
    user_inside = FALSE;
    users_sent = 0;
    current_user_position = USER_NOT_FOUND;

    for (BYTE i = 0; i < UID_SIZE; i++)
        current_user_uid[i] = 0;
    for (BYTE i = 0; i < CONFIG_SIZE; i++)
        current_config[i] = 0;

    KEY_SetUserInside(FALSE);
}

void CONTROLLER_Motor(void)
{
    switch (state)
    {
    case BOOT_INIT_SYSTEM:
        // On system boot - initialize display and LEDs
        SIO_SendMainMenu();
        LCD_WriteNoUserInfo();
        LED_UpdateConfig(current_config); // All zeroes
        state = INPUT_WAIT_DETECT;
        break;

    case INPUT_WAIT_DETECT:
        // Waits until input detected (keypad/RFID/serial)
        cmd_buffer = KEY_GetCommand();
        if (user_inside && cmd_buffer != NO_COMMAND)
        {
            state = KEY_PROCESS_CMD;
            break;
        }

        if (RFID_HasReadUser())
        {
            state = RFID_READ_CARD_DATA;
            break;
        }

        cmd_buffer = SIO_ReadCommand();
        if (cmd_buffer != NO_COMMAND)
        {
            state = SERIAL_PROCESS_CMD;
            break;
        }
        break;

    case KEY_PROCESS_CMD: // On keypad input - process LED update or reset
        if (cmd_buffer == UPDATE_LED)
        {
            KEY_GetUpdateInfo(&led_num, &led_intensity);
            current_config[led_num] = led_intensity;
            state = KEY_STORE_CONFIG;
        }

        if (cmd_buffer == KEYPAD_RESET)
        {
            reset_system();
            state = INPUT_WAIT_DETECT; // Done
        }
        break;

    case KEY_STORE_CONFIG:
        if (EEPROM_StoreConfigForUser(current_user_position, current_config))
        {
            LED_UpdateConfig(current_config);
            LCD_WriteUserInfo(get_last_uid_char(current_user_uid), current_config);
            state = INPUT_WAIT_DETECT;
        }
        break;

    case RFID_READ_CARD_DATA:
        if (RFID_GetReadUserId(rfid_uid))
            state = RFID_VALIDATE_USER;
        break;

    case RFID_VALIDATE_USER:
        user_pos = USER_FindPositionByRFID(rfid_uid);
        if (user_pos == USER_NOT_FOUND)
        {
            SIO_SendUnknownCard(rfid_uid);
            state = INPUT_WAIT_DETECT; // Done
        }
        else if (user_inside && user_pos == current_user_position)
        {
            state = RFID_USER_EXIT;
        }
        else
        {
            state = RFID_USER_ENTER;
        }
        break;

    case RFID_USER_ENTER:
        user_inside = TRUE;
        current_user_position = user_pos;
        for (BYTE i = 0; i < UID_SIZE; i++)
            current_user_uid[i] = rfid_uid[i];

        KEY_SetUserInside(TRUE);
        state = RFID_LOAD_USER_CONFIG;
        break;

    case RFID_LOAD_USER_CONFIG:
        if (EEPROM_ReadConfigForUser(current_user_position, current_config))
        {
            LED_UpdateConfig(current_config);
            SIO_SendDetectedCard(current_user_uid, current_config);
            LCD_WriteUserInfo(get_last_uid_char(current_user_uid), current_config);
            state = INPUT_WAIT_DETECT;
        }
        break;

    case RFID_USER_EXIT:
        user_inside = FALSE;
        current_user_position = USER_NOT_FOUND;
        KEY_SetUserInside(FALSE);

        SIO_SendDetectedCard(current_user_uid, current_config);
        LCD_WriteNoUserInfo();

        clean_config();
        LED_UpdateConfig(current_config);
        state = INPUT_WAIT_DETECT;
        break;

    case SERIAL_PROCESS_CMD:
        switch (cmd_buffer)
        {
        case CMD_WHO_IN_ROOM:
            state = SERIAL_SEND_WHO_RESPONSE;
            break;

        case CMD_SHOW_STORED_CONF:
            state = SERIAL_SEND_CONFIGS;
            break;

        case CMD_UPDATE_TIME:
            SIO_SendTimePrompt();
            state = SERIAL_WAIT_TIME_INPUT;
            break;

        case CMD_ESC:
            SIO_SendMainMenu();
            state = INPUT_WAIT_DETECT;
            break;

        default:
            state = INPUT_WAIT_DETECT;
            break;
        }
        break;

    case SERIAL_SEND_WHO_RESPONSE:
        if (user_inside)
        {
            SIO_SendUser(current_user_uid);
        }
        else
        {
            SIO_SendNoUser();
        }
        state = INPUT_WAIT_DETECT;
        break;

    case SERIAL_SEND_CONFIGS:
        if (EEPROM_ReadConfigForUser(users_sent, current_config))
        {
            SIO_SendStoredConfig(USER_GetUserByPosition(users_sent), current_config);
            users_sent++;
        }

        if (users_sent == NUM_USERS) // Once all users have been sent, reset the counter
        {
            users_sent = 0;
            state = INPUT_WAIT_DETECT;
        }
        break;

    case SERIAL_WAIT_TIME_INPUT:
        if (SIO_ReadTime(&time_hour, &time_minute))
        {
            HORA_SetTime(time_hour, time_minute);
            LCD_UpdateTime(time_hour, time_minute);
            state = INPUT_WAIT_DETECT;
        }
        break;

    default:
        state = INPUT_WAIT_DETECT;
        break;
    }
}

/* =======================================
 *        PRIVATE FUNCTION BODIES
 * ======================================= */

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

static void clean_config(void)
{
    current_config[0] = 0x00;
    current_config[1] = 0x00;
    current_config[2] = 0x00;
    current_config[3] = 0x00;
    current_config[4] = 0x00;
    current_config[5] = 0x00;
}

static BYTE get_last_uid_char(const BYTE *uid)
{
    // Get the last hex character from the last byte of the UID
    BYTE last_byte = uid[4];             // Last byte of 5-byte UID
    BYTE last_nibble = last_byte & 0x0F; // Extract lower nibble (last hex digit)

    // Convert to ASCII character
    return (last_nibble < 10) ? ('0' + last_nibble) : ('A' + last_nibble - 10);
}
