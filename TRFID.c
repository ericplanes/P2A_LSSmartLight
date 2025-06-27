#include "TRFID.h"
#include "TTimer.h"
#include "TUserControl.h"
#include "Utils.h"

/* =======================================
 *              CONSTANTS
 * ======================================= */

// MFRC522 Command constants
#define PCD_IDLE 0x00
#define PCD_TRANSCEIVE 0x0C
#define PCD_RESETPHASE 0x0F
#define PCD_CALCCRC 0x03

// PICC constants
#define PICC_REQIDL 0x26
#define PICC_ANTICOLL 0x93
#define PICC_HALT 0x50

// Status constants
#define MI_OK 0
#define MI_NOTAGERR 1
#define MI_ERR 2

// RFID reading timing and retry settings
#define RFID_SCAN_DELAY (ONE_SECOND / 2) // 500ms delay between scans
#define RFID_RETRY_COUNT 15              // Maximum retry attempts
#define TIMEOUT_COUNT 1000               // Reduced timeout for cooperative operation

/* =======================================
 *         PRIVATE VARIABLES
 * ======================================= */

// RFID card reading state machine (expanded for cooperative operation)
static BYTE rfid_state = 0;
static BYTE retry_counter = 0;
static BOOL rfid_card_detected = FALSE;
static BYTE card_uid[5] = {0};
static BYTE card_data_position = 0;

// State machine working variables (for multi-step operations)
static WORD timeout_counter = 0;
static BYTE crc_counter = 0;
static BYTE temp_data[16];
static BYTE temp_len = 0;
static WORD temp_response_len = 0;
static BYTE checksum = 0;
static BYTE halt_cmd[4];

/* =======================================
 *       PRIVATE FUNCTION HEADERS
 * ======================================= */

// Low-level MFRC522 hardware communication
static BYTE mfrc522_read_register(BYTE address);
static void mfrc522_write_register(BYTE address, BYTE value);
static void mfrc522_clear_register_bit(BYTE addr, BYTE mask);
static void mfrc522_set_register_bit(BYTE addr, BYTE mask);

// MFRC522 hardware initialization and control
static void mfrc522_reset_chip(void);
static void mfrc522_antenna_on(void);
static void mfrc522_initialize_chip(void);

// Cooperative helper functions
static void setup_anticollision_command(void);
static void setup_crc_calculation(BYTE *data, BYTE len);
static void setup_halt_command(void);

/* =======================================
 *         PUBLIC FUNCTION BODIES
 * ======================================= */

void RFID_Init(void)
{
  // Configure MFRC522 SPI pins
  DIR_MFRC522_SO = 1;  // MISO input
  DIR_MFRC522_SI = 0;  // MOSI output
  DIR_MFRC522_SCK = 0; // Clock output
  DIR_MFRC522_CS = 0;  // Chip select output
  DIR_MFRC522_RST = 0; // Reset output

  // Initialize MFRC522 chip
  mfrc522_initialize_chip();

  // Reset state machine
  rfid_state = 0;
  retry_counter = 0;
  card_data_position = 0;
  rfid_card_detected = FALSE;
  timeout_counter = 0;
  crc_counter = 0;

  // Reset timer
  TiResetTics(TI_RFID);
}

void RFID_Motor(void)
{
  BYTE n, i;

  switch (rfid_state)
  {
  case 0: // Initialize card detection (PICC_REQIDL)
    retry_counter = RFID_RETRY_COUNT;
    timeout_counter = TIMEOUT_COUNT;

    // Setup MFRC522 for card detection
    mfrc522_write_register(0x0D, 0x07);           // BitFramingReg
    mfrc522_write_register(0x02, 0x77 | 0x80);    // ComIEnReg
    mfrc522_clear_register_bit(0x04, 0x80);       // ComIrqReg
    mfrc522_set_register_bit(0x0A, 0x80);         // FIFOLevelReg
    mfrc522_write_register(0x01, PCD_IDLE);       // CommandReg
    mfrc522_write_register(0x09, PICC_REQIDL);    // FIFODataReg
    mfrc522_write_register(0x01, PCD_TRANSCEIVE); // Start command
    mfrc522_set_register_bit(0x0D, 0x80);         // StartSend

    rfid_state = 1;
    break;

  case 1:                            // Wait for REQIDL response (cooperative)
    n = mfrc522_read_register(0x04); // ComIrqReg
    if (n & 0x30)                    // Check for RxIRq or IdleIRq
    {
      mfrc522_clear_register_bit(0x0D, 0x80); // Stop transmission

      if (!(mfrc522_read_register(0x06) & 0x1B)) // No errors
      {
        // Card detected - move to anticollision
        rfid_state = 2;
      }
      else
      {
        // Error - retry or give up
        rfid_state = 9; // Go to delay state
      }
    }
    else
    {
      // Still waiting - check timeout
      timeout_counter--;
      if (timeout_counter == 0)
      {
        retry_counter--;
        if (retry_counter > 0)
        {
          rfid_state = 0; // Retry
        }
        else
        {
          mfrc522_clear_register_bit(0x0D, 0x80);
          rfid_state = 9; // Give up, go to delay
        }
      }
    }
    break;

  case 2: // Initialize anticollision command
    setup_anticollision_command();
    timeout_counter = TIMEOUT_COUNT;
    rfid_state = 3;
    break;

  case 3:                            // Wait for anticollision response (cooperative)
    n = mfrc522_read_register(0x04); // ComIrqReg
    if (n & 0x30)                    // Check completion
    {
      mfrc522_clear_register_bit(0x0D, 0x80);

      if (!(mfrc522_read_register(0x06) & 0x1B)) // No errors
      {
        // Read response data
        n = mfrc522_read_register(0x0A); // FIFOLevelReg
        if (n > 16)
          n = 16;

        for (i = 0; i < n; i++)
          temp_data[i] = mfrc522_read_register(0x09);
        temp_len = n;

        rfid_state = 4; // Process response
      }
      else
      {
        rfid_state = 9; // Error - go to delay
      }
    }
    else
    {
      timeout_counter--;
      if (timeout_counter == 0)
      {
        mfrc522_clear_register_bit(0x0D, 0x80);
        rfid_state = 9; // Timeout - go to delay
      }
    }
    break;

  case 4: // Process anticollision response and verify checksum
    if (temp_len >= 5)
    {
      // Verify checksum
      checksum = 0;
      for (i = 0; i < 4; i++)
      {
        checksum ^= temp_data[i];
        card_uid[i] = temp_data[i]; // Copy UID
      }

      if (checksum == temp_data[4])
      {
        // Valid UID - prepare HALT command
        card_uid[4] = temp_data[4];
        rfid_card_detected = TRUE;
        card_data_position = 0;
        rfid_state = 5; // Initialize CRC for HALT
      }
      else
      {
        rfid_state = 9; // Checksum error - go to delay
      }
    }
    else
    {
      rfid_state = 9; // Invalid response - go to delay
    }
    break;

  case 5: // Initialize CRC calculation for HALT (cooperative)
    halt_cmd[0] = PICC_HALT;
    halt_cmd[1] = 0;
    setup_crc_calculation(halt_cmd, 2);
    crc_counter = 255; // Timeout for CRC
    rfid_state = 6;
    break;

  case 6:                            // Wait for CRC calculation completion (cooperative)
    n = mfrc522_read_register(0x05); // DivIrqReg
    if (n & 0x04)                    // CRC calculation complete
    {
      halt_cmd[2] = mfrc522_read_register(0x22); // CRCResultRegL
      halt_cmd[3] = mfrc522_read_register(0x21); // CRCResultRegH
      rfid_state = 7;                            // Send HALT command
    }
    else
    {
      crc_counter--;
      if (crc_counter == 0)
      {
        // CRC timeout - skip HALT and go to delay
        rfid_state = 9;
      }
    }
    break;

  case 7: // Send HALT command
    setup_halt_command();
    timeout_counter = TIMEOUT_COUNT;
    rfid_state = 8;
    break;

  case 8:                            // Wait for HALT completion (cooperative)
    n = mfrc522_read_register(0x04); // ComIrqReg
    if (n & 0x30)                    // Command complete
    {
      mfrc522_clear_register_bit(0x0D, 0x80);
      mfrc522_clear_register_bit(0x08, 0x08); // Clean up
      TiResetTics(TI_RFID);
      rfid_state = 9;
    }
    else
    {
      timeout_counter--;
      if (timeout_counter == 0)
      {
        mfrc522_clear_register_bit(0x0D, 0x80);
        mfrc522_clear_register_bit(0x08, 0x08);
        TiResetTics(TI_RFID);
        rfid_state = 9;
      }
    }
    break;

  case 9: // Delay between scans
    if (TiGetTics(TI_RFID) >= RFID_SCAN_DELAY)
    {
      rfid_state = 0;
    }
    break;
  }
}

BOOL RFID_HasReadUser(void)
{
  return rfid_card_detected;
}

BOOL RFID_GetReadUserId(BYTE *user_uid_buffer)
{
  if (!rfid_card_detected)
    return FALSE;

  // Cooperative data transfer
  if (card_data_position < 5)
  {
    user_uid_buffer[card_data_position] = card_uid[card_data_position];
    card_data_position++;
    return FALSE;
  }

  // Transfer complete
  rfid_card_detected = FALSE;
  card_data_position = 0;
  return TRUE;
}

/* =======================================
 *        PRIVATE FUNCTION BODIES
 * ======================================= */

static BYTE mfrc522_read_register(BYTE address)
{
  BYTE i, result = 0;
  BYTE addr = ((address << 1) & 0x7E) | 0x80;

  MFRC522_SCK = 0;
  MFRC522_CS = 0;

  // Send address
  for (i = 0; i < 8; i++)
  {
    MFRC522_SI = (addr & 0x80) ? 1 : 0;
    MFRC522_SCK = 1;
    addr <<= 1;
    MFRC522_SCK = 0;
  }

  // Read data
  for (i = 0; i < 8; i++)
  {
    MFRC522_SCK = 1;
    result <<= 1;
    if (MFRC522_SO)
      result |= 1;
    MFRC522_SCK = 0;
  }

  MFRC522_CS = 1;
  MFRC522_SCK = 1;
  return result;
}

static void mfrc522_write_register(BYTE address, BYTE value)
{
  BYTE i;
  BYTE addr = ((address << 1) & 0x7E);

  MFRC522_SCK = 0;
  MFRC522_CS = 0;

  // Send address
  for (i = 0; i < 8; i++)
  {
    MFRC522_SI = (addr & 0x80) ? 1 : 0;
    MFRC522_SCK = 1;
    addr <<= 1;
    MFRC522_SCK = 0;
  }

  // Send data
  for (i = 0; i < 8; i++)
  {
    MFRC522_SI = (value & 0x80) ? 1 : 0;
    MFRC522_SCK = 1;
    value <<= 1;
    MFRC522_SCK = 0;
  }

  MFRC522_CS = 1;
  MFRC522_SCK = 1;
}

static void mfrc522_clear_register_bit(BYTE addr, BYTE mask)
{
  BYTE tmp = mfrc522_read_register(addr);
  mfrc522_write_register(addr, tmp & ~mask);
}

static void mfrc522_set_register_bit(BYTE addr, BYTE mask)
{
  BYTE tmp = mfrc522_read_register(addr);
  mfrc522_write_register(addr, tmp | mask);
}

static void mfrc522_reset_chip(void)
{
  MFRC522_RST = 1;
  MFRC522_RST = 0;
  MFRC522_RST = 1;
  mfrc522_write_register(0x01, PCD_RESETPHASE);
}

static void mfrc522_antenna_on(void)
{
  mfrc522_set_register_bit(0x14, 0x03);
}

static void mfrc522_initialize_chip(void)
{
  MFRC522_CS = 1;
  MFRC522_RST = 1;
  mfrc522_reset_chip();
  mfrc522_write_register(0x2A, 0x8D);
  mfrc522_write_register(0x2B, 0x3E);
  mfrc522_write_register(0x2D, 30);
  mfrc522_write_register(0x2C, 0);
  mfrc522_write_register(0x15, 0x40);
  mfrc522_write_register(0x11, 0x3D);
  mfrc522_clear_register_bit(0x14, 0x03); // Antenna off
  mfrc522_antenna_on();
}

static void setup_anticollision_command(void)
{
  mfrc522_write_register(0x0D, 0x00);        // BitFramingReg
  mfrc522_write_register(0x02, 0x77 | 0x80); // ComIEnReg
  mfrc522_clear_register_bit(0x04, 0x80);    // ComIrqReg
  mfrc522_set_register_bit(0x0A, 0x80);      // FIFOLevelReg
  mfrc522_write_register(0x01, PCD_IDLE);    // CommandReg

  // Load anticollision command
  mfrc522_write_register(0x09, PICC_ANTICOLL);
  mfrc522_write_register(0x09, 0x20);

  mfrc522_clear_register_bit(0x08, 0x08); // Clear CollReg
  mfrc522_write_register(0x01, PCD_TRANSCEIVE);
  mfrc522_set_register_bit(0x0D, 0x80); // StartSend
}

static void setup_crc_calculation(BYTE *data, BYTE len)
{
  BYTE i;
  mfrc522_clear_register_bit(0x05, 0x04); // Clear CRCIRq
  mfrc522_set_register_bit(0x0A, 0x80);   // FlushBuffer

  // Load data into FIFO
  for (i = 0; i < len; i++)
    mfrc522_write_register(0x09, data[i]);

  mfrc522_write_register(0x01, PCD_CALCCRC); // Start CRC calculation
}

static void setup_halt_command(void)
{
  mfrc522_write_register(0x02, 0x77 | 0x80); // ComIEnReg
  mfrc522_clear_register_bit(0x04, 0x80);    // ComIrqReg
  mfrc522_set_register_bit(0x0A, 0x80);      // FIFOLevelReg
  mfrc522_write_register(0x01, PCD_IDLE);    // CommandReg

  // Load HALT command with CRC
  mfrc522_write_register(0x09, halt_cmd[0]);
  mfrc522_write_register(0x09, halt_cmd[1]);
  mfrc522_write_register(0x09, halt_cmd[2]);
  mfrc522_write_register(0x09, halt_cmd[3]);

  mfrc522_clear_register_bit(0x08, 0x80);
  mfrc522_write_register(0x01, PCD_TRANSCEIVE);
  mfrc522_set_register_bit(0x0D, 0x80); // StartSend
}
