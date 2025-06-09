#include "TEEPROM.h"

#define LOG_SIZE 15 - 1
#define MAX_LOGS 15

#define EEPROM_IDLE 0
#define EEPROM_WRITING 1
#define EEPROM_READING 2

static BYTE write_pos = 0; // Separate position counter for writing
static BYTE read_pos = 0;  // Separate position counter for reading
static BYTE eeprom_state = EEPROM_IDLE;

/* =======================================
 *       PRIVATE FUNCTION HEADERS
 * ======================================= */
static BYTE read_byte(BYTE address);
static void prepare_write_info(BYTE address, BYTE data);
static void write_prepared_info(void);
static void write_byte(BYTE address, BYTE data);

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
    // Reset state variables
    write_pos = 0;
    read_pos = 0;
    eeprom_state = EEPROM_IDLE;

    // Total EEPROM space used:
    BYTE total_bytes = MAX_LOGS * LOG_SIZE;

    // Clean all used EEPROM bytes
    for (BYTE addr = 0; addr < total_bytes; addr++)
    {
        write_byte(addr, 0x00);
    }
}

BOOL EEPROM_StoreLog(const BYTE *log_data)
{
    if (eeprom_state == EEPROM_READING || EECON1bits.WR)
        return FALSE;

    eeprom_state = EEPROM_WRITING;

    if (write_pos < LOG_SIZE)
    {
        write_byte(write_pos, log_data[write_pos]);
        write_pos++;
    }

    if (write_pos == LOG_SIZE)
    {
        write_pos = 0;
        eeprom_state = EEPROM_IDLE;
        return TRUE;
    }

    return FALSE;
}

BOOL EEPROM_ReadLog(BYTE section, BYTE *log_data)
{
    if (eeprom_state == EEPROM_WRITING)
        return FALSE;

    eeprom_state = EEPROM_READING;

    if (read_pos < LOG_SIZE)
    {
        log_data[read_pos] = read_byte(read_pos + (section * LOG_SIZE));
        read_pos++;
    }

    if (read_pos == LOG_SIZE)
    {
        log_data[read_pos] = '\0';
        eeprom_state = EEPROM_IDLE;
        read_pos = 0;
        return TRUE;
    }

    return FALSE;
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
