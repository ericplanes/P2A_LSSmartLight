#include "TController.h"
#include "TSerial.h"
#include "TLCD.h"
#include "TKeypad.h"
#include "TLight.h"
#include "TRFID.h"
#include "TEEPROM.h"
#include "TUserControl.h"
#include "THora.h"

/* =======================================
 *              CONSTANTS
 * ======================================= */

#define UID_SIZE 5
#define UID_STRING_SIZE 15 // "11-22-33-44-55\0"
#define CONFIG_SIZE 6

/* =======================================
 *         PRIVATE VARIABLES
 * ======================================= */

static BOOL user_inside;
static BYTE current_user_uid[UID_SIZE];
static BYTE current_user_position;
static BYTE current_config[CONFIG_SIZE];
static BYTE zero_config[CONFIG_SIZE] = {0, 0, 0, 0, 0, 0};

/* =======================================
 *       PRIVATE FUNCTION HEADERS
 * ======================================= */

static void format_uid_to_string(BYTE *uid, BYTE *formatted_string);
static void reset_system_state(void);
static void copy_uid(BYTE *source, BYTE *destination);
static BYTE hex_to_char(BYTE hex_value);

/* =======================================
 *         PUBLIC FUNCTION BODIES
 * ======================================= */

void CONTROLLER_Init(void)
{
    // Initialize internal state
    user_inside = FALSE;
    current_user_position = USER_NOT_FOUND;

    // Clear UIDs and config
    for (BYTE i = 0; i < UID_SIZE; i++)
        current_user_uid[i] = 0;
    for (BYTE i = 0; i < CONFIG_SIZE; i++)
        current_config[i] = 0;

    // Startup protocol
    SIO_SendMainMenu();
    LCD_WriteNoUserInfo();
}

void CONTROLLER_Motor(void)
{
    BYTE keypad_command;
    BYTE led_number, led_intensity;
    BYTE rfid_uid[UID_SIZE];
    BYTE formatted_uid[UID_STRING_SIZE];
    BYTE user_position;
    BYTE serial_command;
    BYTE new_hour, new_mins;

    // Step 2: If user is inside, check keypad commands
    if (user_inside)
    {
        keypad_command = KEY_GetCommand();

        if (keypad_command == UPDATE_LED)
        {
            KEY_GetUpdateInfo(&led_number, &led_intensity);

            // Update current config
            current_config[led_number] = led_intensity;

            // Store in EEPROM
            if (EEPROM_StoreConfigForUser(current_user_position, current_config))
            {
                LED_UpdateConfig(current_config);
                LCD_WriteUserInfo(current_user_uid[UID_SIZE - 1], current_config);
            }
        }
        else if (keypad_command == KEYPAD_RESET)
        {
            reset_system_state();
            return; // Start fresh next cycle
        }
        // If no valid keypad command, continue to step 3
    }

    // Step 3: Check RFID (if no user inside OR no keypad command processed)
    if (RFID_HasReadUser())
    {
        if (RFID_GetReadUserId(rfid_uid))
        {
            user_position = USER_FindPositionByRFID(rfid_uid);

            if (user_position == USER_NOT_FOUND)
            {
                // Unknown card
                format_uid_to_string(rfid_uid, formatted_uid);
                SIO_SendUnknownCard(formatted_uid);
            }
            else
            {
                // Known user
                if (user_inside && user_position == current_user_position)
                {
                    // Same user leaving
                    user_inside = FALSE;
                    current_user_position = USER_NOT_FOUND;

                    format_uid_to_string(current_user_uid, formatted_uid);
                    SIO_SendDetectedCard(formatted_uid, current_config);
                    LCD_WriteNoUserInfo();
                    LED_UpdateConfig(zero_config);

                    // Clear current user data
                    for (BYTE i = 0; i < UID_SIZE; i++)
                        current_user_uid[i] = 0;
                    for (BYTE i = 0; i < CONFIG_SIZE; i++)
                        current_config[i] = 0;
                }
                else
                {
                    // New user entering (or different user)
                    user_inside = TRUE;
                    current_user_position = user_position;
                    copy_uid(rfid_uid, current_user_uid);

                    // Read stored configuration
                    EEPROM_ReadConfigForUser(user_position, current_config);

                    // Update system
                    LED_UpdateConfig(current_config);
                    format_uid_to_string(current_user_uid, formatted_uid);
                    SIO_SendDetectedCard(formatted_uid, current_config);
                    LCD_WriteUserInfo(current_user_uid[UID_SIZE - 1], current_config);
                }
            }
        }
    }

    // Step 4: Check serial commands
    serial_command = SIO_ReadCommand();

    switch (serial_command)
    {
    case CMD_WHO_IN_ROOM:
        if (user_inside)
        {
            format_uid_to_string(current_user_uid, formatted_uid);
            SIO_SendUser(formatted_uid);
        }
        else
        {
            SIO_SendNoUser();
        }
        break;

    case CMD_SHOW_STORED_CONF:
        for (BYTE pos = 0; pos < NUM_USERS; pos++)
        {
            const BYTE *stored_uid = USER_GetUserByPosition(pos);
            BYTE stored_config[CONFIG_SIZE];

            if (stored_uid != (const BYTE *)0)
            {
                EEPROM_ReadConfigForUser(pos, stored_config);
                format_uid_to_string((BYTE *)stored_uid, formatted_uid);
                SIO_SendStoredConfig(formatted_uid, stored_config);
            }
        }
        break;

    case CMD_UPDATE_TIME:
        SIO_SendTimePrompt();
        // Wait until time is read
        while (!SIO_ReadTime(&new_hour, &new_mins))
        {
            // Keep checking (cooperative)
            return; // Will continue next cycle
        }
        HORA_SetTime(new_hour, new_mins);
        break;

    case CMD_ESC:
        SIO_SendMainMenu();
        break;

    case CMD_NO_COMMAND:
    default:
        // No action needed
        break;
    }
}

/* =======================================
 *        PRIVATE FUNCTION BODIES
 * ======================================= */

static void format_uid_to_string(BYTE *uid, BYTE *formatted_string)
{
    BYTE pos = 0;

    for (BYTE i = 0; i < UID_SIZE; i++)
    {
        // High nibble
        formatted_string[pos++] = hex_to_char((uid[i] >> 4) & 0x0F);
        // Low nibble
        formatted_string[pos++] = hex_to_char(uid[i] & 0x0F);

        // Add hyphen except after last byte
        if (i < UID_SIZE - 1)
            formatted_string[pos++] = '-';
    }

    formatted_string[pos] = '\0'; // Null terminate
}

static void reset_system_state(void)
{
    // Clear all memory
    EEPROM_CleanMemory();

    // Reset system state
    user_inside = FALSE;
    current_user_position = USER_NOT_FOUND;

    // Clear data
    for (BYTE i = 0; i < UID_SIZE; i++)
        current_user_uid[i] = 0;
    for (BYTE i = 0; i < CONFIG_SIZE; i++)
        current_config[i] = 0;

    // Update hardware
    LED_UpdateConfig(zero_config);
    LCD_WriteNoUserInfo();
}

static void copy_uid(BYTE *source, BYTE *destination)
{
    for (BYTE i = 0; i < UID_SIZE; i++)
        destination[i] = source[i];
}

static BYTE hex_to_char(BYTE hex_value)
{
    if (hex_value < 10)
        return '0' + hex_value;
    else
        return 'A' + (hex_value - 10);
}
