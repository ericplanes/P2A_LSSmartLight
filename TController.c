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

#define INPUT_WAIT_DETECT 0         // Waits until input detected (keypad/RFID/serial)
#define KEY_PROCESS_CMD 1           // On keypad input - process LED update or reset
#define KEY_STORE_CONFIG 2          // After LED update - store to EEPROM
#define RFID_READ_CARD_DATA 3       // On card detected - read UID data
#define RFID_VALIDATE_USER 4        // On UID complete - validate against known users
#define RFID_USER_EXIT 5            // On same user card - user leaving
#define RFID_LOAD_NEW_USER_CONFIG 6 // After user validation - load their config
#define SERIAL_PROCESS_CMD 7        // On serial input - process menu commands
#define SERIAL_SEND_WHO_RESPONSE 8  // On "who in room" - send current user
#define SERIAL_SEND_CONFIGS 9       // On "show configs" - send all stored configs
#define SERIAL_WAIT_TIME_INPUT 10   // After time request - wait for time data

/* =======================================
 *         PRIVATE VARIABLES
 * ======================================= */

static BYTE state;
static BYTE current_user_position;
static BYTE current_config[CONFIG_SIZE];
static BYTE time_hour, time_minute;

// Processing variables
static BYTE rfid_uid[UID_SIZE];
static BYTE command_read;
static BYTE led_num, led_intensity;
static BYTE user_pos, last_uid_char;

/* =======================================
 *       PRIVATE FUNCTION HEADERS
 * ======================================= */

static void reset_system(void);
static void clean_config(void);
static BYTE get_last_uid_char(const BYTE *uid);
static void clean_uid(void);
static void init_controller_variables(void);
static void finish_comand(void);

/* =======================================
 *         PUBLIC FUNCTION BODIES
 * ======================================= */

void CNTR_Init(void)
{
    init_controller_variables();
    SIO_SendMainMenu();
    LCD_WriteNoUserInfo();
    TiResetTics(TI_TEST);
}

void CNTR_Motor(void)
{
    switch (state)
    {
    case INPUT_WAIT_DETECT: // Waits until input detected (keypad/RFID/serial)
        command_read = KEY_GetCommand();
        if (command_read != KEY_NO_COMMAND && command_read != CMD_NO_COMMAND)
        {
            state = KEY_PROCESS_CMD;
            break;
        }

        if (RFID_HasReadUser())
        {
            state = RFID_READ_CARD_DATA;
            break;
        }

        command_read = SIO_ReadCommand();
        if (command_read != KEY_NO_COMMAND && command_read != CMD_NO_COMMAND)
        {
            state = SERIAL_PROCESS_CMD;
            break;
        }
        break;

    case KEY_PROCESS_CMD: // On keypad input - process LED update or reset
        if (command_read == UPDATE_LED)
        {
            KEY_GetUpdateInfo(&led_num, &led_intensity);
            current_config[led_num] = led_intensity;
            state = KEY_STORE_CONFIG;
        }
        else // KEY_GetCommand() == KEYPAD_RESET
        {
            reset_system();
            finish_comand();
        }
        break;

    case KEY_STORE_CONFIG:
        if (EEPROM_StoreConfigForUser(current_user_position, current_config))
        {
            LED_UpdateConfig(current_config);
            LCD_WriteUserInfo(last_uid_char, current_config);
            finish_comand();
        }
        break;

    case RFID_READ_CARD_DATA:
        if (RFID_GetReadUserId(rfid_uid))
        {
            state = RFID_VALIDATE_USER;
        }
        break;

    case RFID_VALIDATE_USER:
        user_pos = USER_FindPositionByRFID(rfid_uid);
        if (user_pos == USER_NOT_FOUND)
        {
            SIO_SendUnknownCard(rfid_uid);
            finish_comand();
        }
        else if (user_pos == current_user_position)
        {
            last_uid_char = '-';
            state = RFID_USER_EXIT;
        }
        else
        {
            last_uid_char = get_last_uid_char(rfid_uid);
            current_user_position = user_pos;
            KEY_SetUserInside(TRUE);
            state = RFID_LOAD_NEW_USER_CONFIG;
        }
        break;

    case RFID_LOAD_NEW_USER_CONFIG:
        if (EEPROM_ReadConfigForUser(current_user_position, current_config))
        {
            LED_UpdateConfig(current_config);
            SIO_SendDetectedCard(rfid_uid, current_config);
            LCD_WriteUserInfo(get_last_uid_char(rfid_uid), current_config);
            finish_comand();
        }
        break;

    case RFID_USER_EXIT:
        current_user_position = USER_NOT_FOUND;
        KEY_SetUserInside(FALSE);

        SIO_SendDetectedCard(rfid_uid, current_config);
        LCD_WriteNoUserInfo();

        clean_config();
        LED_UpdateConfig(current_config);
        finish_comand();
        break;

    case SERIAL_PROCESS_CMD:
        switch (command_read)
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
            finish_comand();
            break;

        default:
            finish_comand();
            break;
        }
        break;

    case SERIAL_SEND_WHO_RESPONSE:
        if (current_user_position != USER_NOT_FOUND)
        {
            SIO_SendUser(rfid_uid);
        }
        else
        {
            SIO_SendNoUser();
        }
        finish_comand();
        break;

    case SERIAL_SEND_CONFIGS:
        for (BYTE user = 0; user < NUM_USERS; user++)
        {
            if (EEPROM_ReadConfigForUser(user, current_config))
            {
                SIO_SendStoredConfig(USER_GetUserByPosition(user), current_config);
                user++;
            }
        }
        finish_comand();
        break;

    case SERIAL_WAIT_TIME_INPUT:
        if (SIO_ReadTime(&time_hour, &time_minute))
        {
            HORA_SetTime(time_hour, time_minute);
            LCD_UpdateTime(time_hour, time_minute);
            finish_comand();
        }
        break;
    }
}

/* =======================================
 *        PRIVATE FUNCTION BODIES
 * ======================================= */

static void reset_system(void)
{
    EEPROM_CleanMemory();
    current_user_position = USER_NOT_FOUND;

    clean_uid();
    clean_config();

    LED_UpdateConfig(current_config);
    LCD_WriteNoUserInfo();
    KEY_SetUserInside(FALSE);
}

static void clean_uid(void)
{
    rfid_uid[0] = 0x00;
    rfid_uid[1] = 0x00;
    rfid_uid[2] = 0x00;
    rfid_uid[3] = 0x00;
    rfid_uid[4] = 0x00;
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

    if (last_nibble < 10)
    {
        return '0' + last_nibble;
    }
    return 'A' + last_nibble - 10;
}

static void init_controller_variables(void)
{
    state = INPUT_WAIT_DETECT;
    current_user_position = USER_NOT_FOUND;
    time_hour = 0;
    time_minute = 0;
    command_read = KEY_NO_COMMAND;
    led_num = 0;
    led_intensity = 0;
    user_pos = 0;
    last_uid_char = '-';

    // Initialize arrays
    clean_uid();
    clean_config();
}

static void finish_comand(void)
{
    command_read = KEY_NO_COMMAND;
    state = INPUT_WAIT_DETECT;
}
