#include "TEEPROM.h"

#define NUM_LEDS 6
#define MAX_USERS 42 // 256 bytes EEPROM / 6 bytes per user = 42 users max

static BYTE write_pos = 0;
static BYTE read_pos = 0;
static BYTE base_address;
static BYTE current_user;

/* =======================================
 *       PRIVATE FUNCTION HEADERS
 * ======================================= */
static BYTE read_byte(BYTE address);
static void prepare_write_info(BYTE address, BYTE data);
static void write_prepared_info(void);
static void write_byte(BYTE address, BYTE data);
static void check_user(BYTE user);

/* =======================================
 *          PUBLIC FUNCTION BODIES
 * ======================================= */

void EEPROM_Init(void)
{
    write_pos = 0;
    read_pos = 0;
    current_user = 0xFF;
}

void EEPROM_CleanMemory(void)
{
    // Reset state variables
    write_pos = 0;
    read_pos = 0;
    current_user = 0xFF;

    // Total EEPROM space used:
    BYTE total_bytes = MAX_USERS * NUM_LEDS;

    // Clean all used EEPROM bytes
    for (BYTE addr = 0; addr < total_bytes; addr++)
    {
        write_byte(addr, 0x00);
    }
}

BOOL EEPROM_StoreConfigForUser(BYTE user, const BYTE *led_config)
{
    check_user(user);

    if (write_pos < NUM_LEDS)
    {
        write_byte(base_address + write_pos, led_config[write_pos]);
        write_pos++;
    }

    if (write_pos == NUM_LEDS)
    {
        write_pos = 0;
        return TRUE;
    }

    return FALSE;
}

BOOL EEPROM_ReadConfigForUser(BYTE user, BYTE *led_config)
{
    check_user(user);

    if (read_pos < NUM_LEDS)
    {
        led_config[read_pos] = read_byte(base_address + read_pos);
        read_pos++;
    }

    if (read_pos == NUM_LEDS)
    {
        read_pos = 0;
        return TRUE;
    }

    return FALSE;
}

/* =======================================
 *          PRIVATE FUNCTION BODIES
 * ======================================= */

static void check_user(BYTE user)
{
    if (user != current_user)
    {
        current_user = user;
        base_address = user * NUM_LEDS;
        write_pos = 0;
        read_pos = 0;
    }
}

static BYTE read_byte(BYTE address)
{
    EEADR = address;
    EECON1bits.EEPGD = 0;
    EECON1bits.CFGS = 0;
    EECON1bits.RD = 1;
    return EEDATA;
}

static void prepare_write_info(BYTE address, BYTE data)
{
    EECON1bits.WREN = 1;
    EEADR = address;
    EEDATA = data;
}

static void write_prepared_info(void)
{
    EECON1bits.EEPGD = 0; // Data EEPROM
    EECON1bits.CFGS = 0;  // Access EEPROM
    EECON1bits.WREN = 1;

    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1; // Start write

    while (EECON1bits.WR)
        ;                // Wait for WR to become 0 (end of write operation)
    PIR2bits.EEIF = 0;   // Clear the write flag
    EECON1bits.WREN = 0; // Disable write
}

static void write_byte(BYTE address, BYTE data)
{
    prepare_write_info(address, data);
    di();
    write_prepared_info();
    ei();
}
