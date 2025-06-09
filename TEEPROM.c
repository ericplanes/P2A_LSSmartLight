#include "TEEPROM.h"

#define NUM_LIGHTS 6
#define MAX_USERS 8 // Adjust based on available EEPROM space
#define CONFIG_SIZE NUM_LIGHTS
#define USER_CONFIG_SIZE (CONFIG_SIZE + 1) // +1 for validity flag

#define EEPROM_IDLE 0
#define EEPROM_WRITING 1
#define EEPROM_READING 2

#define VALID_CONFIG_FLAG 0xAA
#define INVALID_CONFIG_FLAG 0x00

static BYTE write_pos = 0;
static BYTE read_pos = 0;
static BYTE eeprom_state = EEPROM_IDLE;

/* =======================================
 *       PRIVATE FUNCTION HEADERS
 * ======================================= */
static BYTE read_byte(BYTE address);
static void write_byte(BYTE address, BYTE data);
static BYTE get_user_config_address(BYTE uid_index);

/* =======================================
 *          PUBLIC FUNCTION BODIES
 * ======================================= */

void EEPROM_Init(void)
{
    write_pos = 0;
    read_pos = 0;
    eeprom_state = EEPROM_IDLE;
}

void EEPROM_CleanMemory(void)
{
    eeprom_state = EEPROM_IDLE;
    write_pos = 0;
    read_pos = 0;

    // Clean all user configuration space
    BYTE total_bytes = MAX_USERS * USER_CONFIG_SIZE;
    for (BYTE addr = 0; addr < total_bytes; addr++)
    {
        write_byte(addr, INVALID_CONFIG_FLAG);
    }
}

BOOL EEPROM_StoreUserConfig(BYTE uid_index, BYTE *light_config)
{
    if (uid_index >= MAX_USERS || eeprom_state == EEPROM_READING || EECON1bits.WR)
        return FALSE;

    eeprom_state = EEPROM_WRITING;
    BYTE base_addr = get_user_config_address(uid_index);

    if (write_pos < CONFIG_SIZE)
    {
        write_byte(base_addr + write_pos + 1, light_config[write_pos]); // +1 to skip validity flag
        write_pos++;
    }

    if (write_pos == CONFIG_SIZE)
    {
        // Mark configuration as valid
        write_byte(base_addr, VALID_CONFIG_FLAG);
        write_pos = 0;
        eeprom_state = EEPROM_IDLE;
        return TRUE;
    }

    return FALSE;
}

BOOL EEPROM_ReadUserConfig(BYTE uid_index, BYTE *light_config)
{
    if (uid_index >= MAX_USERS || eeprom_state == EEPROM_WRITING)
        return FALSE;

    eeprom_state = EEPROM_READING;
    BYTE base_addr = get_user_config_address(uid_index);

    // Check if configuration is valid
    if (read_byte(base_addr) != VALID_CONFIG_FLAG)
    {
        eeprom_state = EEPROM_IDLE;
        read_pos = 0;
        return FALSE;
    }

    if (read_pos < CONFIG_SIZE)
    {
        light_config[read_pos] = read_byte(base_addr + read_pos + 1); // +1 to skip validity flag
        read_pos++;
    }

    if (read_pos == CONFIG_SIZE)
    {
        eeprom_state = EEPROM_IDLE;
        read_pos = 0;
        return TRUE;
    }

    return FALSE;
}

BOOL EEPROM_IsConfigurationStored(BYTE uid_index)
{
    if (uid_index >= MAX_USERS)
        return FALSE;

    BYTE base_addr = get_user_config_address(uid_index);
    return (read_byte(base_addr) == VALID_CONFIG_FLAG);
}

/* =======================================
 *          PRIVATE FUNCTION BODIES
 * ======================================= */

static BYTE read_byte(BYTE address)
{
    EEADR = address;
    EECON1bits.EEPGD = 0;
    EECON1bits.CFGS = 0;
    EECON1bits.RD = 1;
    return EEDATA;
}

static void write_byte(BYTE address, BYTE data)
{
    EECON1bits.WREN = 1;
    EEADR = address;
    EEDATA = data;
    EECON1bits.EEPGD = 0; // Data EEPROM
    EECON1bits.CFGS = 0;  // Access EEPROM

    di();
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1; // Start write

    while (EECON1bits.WR)
        ;                // Wait for WR to become 0 (end of write operation)
    PIR2bits.EEIF = 0;   // Clear the write flag
    EECON1bits.WREN = 0; // Disable write
    ei();
}

static BYTE get_user_config_address(BYTE uid_index)
{
    return uid_index * USER_CONFIG_SIZE;
}
