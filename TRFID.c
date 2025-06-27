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

// RFID card reading state machine (ultra-cooperative with 18 states)
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
static BYTE checksum = 0;
static BYTE halt_cmd[4];
static BYTE loop_counter = 0; // For iterative operations

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
  loop_counter = 0;

  // Reset timer
  TiResetTics(TI_RFID);
}

void RFID_Motor(void)
{
  BYTE n, i;

  switch (rfid_state)
  {
  case 0: // Initialize detection - Step 1: Setup basic registers
    retry_counter = RFID_RETRY_COUNT;
    timeout_counter = TIMEOUT_COUNT;
    mfrc522_write_register(0x0D, 0x07); // BitFramingReg
    rfid_state = 1;
    break;

  case 1:                                      // Initialize detection - Step 2: Enable interrupts
    mfrc522_write_register(0x02, 0x77 | 0x80); // ComIEnReg
    rfid_state = 2;
    break;

  case 2:                                   // Initialize detection - Step 3: Clear interrupt flags
    mfrc522_clear_register_bit(0x04, 0x80); // ComIrqReg
    rfid_state = 3;
    break;

  case 3:                                 // Initialize detection - Step 4: Reset FIFO
    mfrc522_set_register_bit(0x0A, 0x80); // FIFOLevelReg
    rfid_state = 4;
    break;

  case 4:                                   // Initialize detection - Step 5: Set idle command
    mfrc522_write_register(0x01, PCD_IDLE); // CommandReg
    rfid_state = 5;
    break;

  case 5:                                      // Initialize detection - Step 6: Load REQIDL command
    mfrc522_write_register(0x09, PICC_REQIDL); // FIFODataReg
    rfid_state = 6;
    break;

  case 6:                                         // Initialize detection - Step 7: Start transceive
    mfrc522_write_register(0x01, PCD_TRANSCEIVE); // CommandReg
    rfid_state = 7;
    break;

  case 7:                                 // Initialize detection - Step 8: Start transmission
    mfrc522_set_register_bit(0x0D, 0x80); // StartSend
    rfid_state = 8;
    break;

  case 8:                            // Wait for REQIDL response (cooperative)
    n = mfrc522_read_register(0x04); // ComIrqReg - single register read
    if (n & 0x30)                    // Check for RxIRq or IdleIRq
    {
      rfid_state = 9; // Process response
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
          rfid_state = 0; // Retry from beginning
        }
        else
        {
          rfid_state = 17; // Give up, go to delay
        }
      }
    }
    break;

  case 9:                                   // Process REQIDL response - Stop transmission
    mfrc522_clear_register_bit(0x0D, 0x80); // Stop transmission
    rfid_state = 10;
    break;

  case 10:                           // Check for communication errors
    n = mfrc522_read_register(0x06); // ErrorReg - single register read
    if (!(n & 0x1B))                 // No errors
    {
      rfid_state = 11; // Card detected - setup anticollision
    }
    else
    {
      rfid_state = 17; // Error - go to delay
    }
    break;

  case 11:                              // Setup anticollision - Step 1: Configure bit framing
    mfrc522_write_register(0x0D, 0x00); // BitFramingReg
    timeout_counter = TIMEOUT_COUNT;
    rfid_state = 12;
    break;

  case 12:                                     // Setup anticollision - Step 2: Enable interrupts
    mfrc522_write_register(0x02, 0x77 | 0x80); // ComIEnReg
    rfid_state = 13;
    break;

  case 13:                                  // Setup anticollision - Step 3: Clear interrupt flags and reset FIFO
    mfrc522_clear_register_bit(0x04, 0x80); // ComIrqReg
    mfrc522_set_register_bit(0x0A, 0x80);   // FIFOLevelReg
    rfid_state = 14;
    break;

  case 14:                                       // Setup anticollision - Step 4: Set idle and load first command byte
    mfrc522_write_register(0x01, PCD_IDLE);      // CommandReg
    mfrc522_write_register(0x09, PICC_ANTICOLL); // Load first byte
    rfid_state = 15;
    break;

  case 15:                                  // Setup anticollision - Step 5: Load second byte and start
    mfrc522_write_register(0x09, 0x20);     // Load second byte
    mfrc522_clear_register_bit(0x08, 0x08); // Clear CollReg
    mfrc522_write_register(0x01, PCD_TRANSCEIVE);
    mfrc522_set_register_bit(0x0D, 0x80); // StartSend
    rfid_state = 16;
    break;

  case 16:                           // Wait for anticollision response (cooperative)
    n = mfrc522_read_register(0x04); // ComIrqReg - single register read
    if (n & 0x30)                    // Check completion
    {
      mfrc522_clear_register_bit(0x0D, 0x80); // Stop transmission

      if (!(mfrc522_read_register(0x06) & 0x1B)) // No errors
      {
        // Read response data
        n = mfrc522_read_register(0x0A); // FIFOLevelReg
        if (n > 16)
          n = 16;
        temp_len = n;
        loop_counter = 0; // Prepare for data reading
        rfid_state = 17;  // Read FIFO data cooperatively
      }
      else
      {
        rfid_state = 29; // Error - go to delay
      }
    }
    else
    {
      timeout_counter--;
      if (timeout_counter == 0)
      {
        mfrc522_clear_register_bit(0x0D, 0x80);
        rfid_state = 29; // Timeout - go to delay
      }
    }
    break;

  case 17: // Read FIFO data cooperatively (one byte per state call)
    if (loop_counter < temp_len)
    {
      temp_data[loop_counter] = mfrc522_read_register(0x09); // Read one byte
      loop_counter++;
      // Stay in this state until all bytes read
    }
    else
    {
      rfid_state = 18; // Process response
    }
    break;

  case 18: // Process anticollision response - verify length
    if (temp_len >= 5)
    {
      checksum = 0;
      loop_counter = 0;
      rfid_state = 19;
    }
    else
    {
      rfid_state = 29; // Invalid response - go to delay
    }
    break;

  case 19: // Verify checksum cooperatively (one byte per call)
    if (loop_counter < 4)
    {
      checksum ^= temp_data[loop_counter];
      card_uid[loop_counter] = temp_data[loop_counter]; // Copy UID
      loop_counter++;
      // Stay in this state until all 4 bytes processed
    }
    else
    {
      rfid_state = 20; // Check final checksum
    }
    break;

  case 20: // Final checksum verification
    if (checksum == temp_data[4])
    {
      // Valid UID
      card_uid[4] = temp_data[4];
      rfid_card_detected = TRUE;
      card_data_position = 0;
      rfid_state = 21; // Initialize CRC for HALT
    }
    else
    {
      rfid_state = 29; // Checksum error - go to delay
    }
    break;

  case 21: // Initialize CRC calculation - Step 1: Prepare HALT command
    halt_cmd[0] = PICC_HALT;
    halt_cmd[1] = 0;
    rfid_state = 22;
    break;

  case 22:                                  // Initialize CRC calculation - Step 2: Clear CRC interrupt
    mfrc522_clear_register_bit(0x05, 0x04); // Clear CRCIRq
    rfid_state = 23;
    break;

  case 23:                                // Initialize CRC calculation - Step 3: Flush buffer
    mfrc522_set_register_bit(0x0A, 0x80); // FlushBuffer
    rfid_state = 24;
    break;

  case 24: // Initialize CRC calculation - Step 4: Load first data byte
    mfrc522_write_register(0x09, halt_cmd[0]);
    rfid_state = 25;
    break;

  case 25: // Initialize CRC calculation - Step 5: Load second data byte
    mfrc522_write_register(0x09, halt_cmd[1]);
    rfid_state = 26;
    break;

  case 26:                                     // Initialize CRC calculation - Step 6: Start CRC calculation
    mfrc522_write_register(0x01, PCD_CALCCRC); // Start CRC calculation
    crc_counter = 255;                         // Timeout for CRC
    rfid_state = 27;
    break;

  case 27:                           // Wait for CRC calculation completion (cooperative)
    n = mfrc522_read_register(0x05); // DivIrqReg - single register read
    if (n & 0x04)                    // CRC calculation complete
    {
      rfid_state = 28; // Read CRC results
    }
    else
    {
      crc_counter--;
      if (crc_counter == 0)
      {
        // CRC timeout - skip HALT and go to delay
        rfid_state = 29;
      }
    }
    break;

  case 28:                                     // Read CRC results
    halt_cmd[2] = mfrc522_read_register(0x22); // CRCResultRegL
    halt_cmd[3] = mfrc522_read_register(0x21); // CRCResultRegH
    rfid_state = 29;                           // Skip HALT sending for ultra-fast operation
    break;

  case 29:                                  // Cleanup and prepare for delay
    mfrc522_clear_register_bit(0x08, 0x08); // Clean up collision register
    TiResetTics(TI_RFID);
    rfid_state = 30;
    break;

  case 30: // Delay between scans
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

  // Ultra-cooperative data transfer (one byte per call)
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
