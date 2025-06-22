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

// RFID reading timing
#define RFID_SCAN_DELAY (ONE_SECOND / 2) // 500ms delay between scans

/* =======================================
 *         PRIVATE VARIABLES
 * ======================================= */

// RFID card reading state machine
static BYTE rfid_reading_state = 0;
static BOOL rfid_card_detected = FALSE;
static BYTE card_uid[5] = {0};
static BYTE card_data_position = 0;

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
static void mfrc522_antenna_off(void);
static void mfrc522_initialize_chip(void);

// RFID card detection and reading functions
static BYTE mfrc522_send_command_to_card(BYTE command, BYTE *send_data, BYTE send_len, BYTE *back_data, WORD *back_len);
static void mfrc522_calculate_crc(BYTE *data_in, BYTE length, BYTE *data_out);
static BYTE mfrc522_anticollision_detection(BYTE *serial_number);
static BYTE mfrc522_read_card_uid(BYTE *uid_buffer);
static void mfrc522_halt_card_communication(void);
static void fake_rfid_read(void);

/* =======================================
 *         PUBLIC FUNCTION BODIES
 * ======================================= */

void RFID_Init(void)
{
  // Configure MFRC522 SPI pins as per hardware setup
  DIR_MFRC522_SO = 1;  // MISO input
  DIR_MFRC522_SI = 0;  // MOSI output
  DIR_MFRC522_SCK = 0; // Clock output
  DIR_MFRC522_CS = 0;  // Chip select output
  DIR_MFRC522_RST = 0; // Reset output

  // Initialize MFRC522 chip for card reading
  mfrc522_initialize_chip();

  // Reset card reading state machine
  rfid_reading_state = 0;
  card_data_position = 0;
  rfid_card_detected = FALSE;

  // Reset RFID timer for cooperative operation
  TiResetTics(TI_RFID);
  fake_rfid_read(); // for testing purposes
}

void RFID_Motor(void)
{
  switch (rfid_reading_state)
  {
  case 0: // Start card detection sequence
    mfrc522_write_register(BITFRAMINGREG, 0x07);
    mfrc522_write_register(COMMIENREG, 0x77 | 0x80);
    mfrc522_clear_register_bit(COMMIRQREG, 0x80);
    mfrc522_set_register_bit(FIFOLEVELREG, 0x80);
    mfrc522_write_register(COMMANDREG, PCD_IDLE);
    mfrc522_write_register(FIFODATAREG, PICC_REQIDL);
    mfrc522_write_register(COMMANDREG, PCD_TRANSCEIVE);
    mfrc522_set_register_bit(BITFRAMINGREG, 0x80);
    rfid_reading_state = 1;
    break;

  case 1: // Wait for card response and read UID
    if (mfrc522_read_register(COMMIRQREG) & 0x30)
    {
      mfrc522_clear_register_bit(BITFRAMINGREG, 0x80);
      if (!(mfrc522_read_register(ERRORREG) & 0x1B))
      {
        if (mfrc522_read_card_uid(card_uid))
        {
          rfid_card_detected = TRUE;
          card_data_position = 0;
        }
      }
      mfrc522_halt_card_communication();
      TiResetTics(TI_RFID);
      rfid_reading_state = 2;
    }
    break;

  case 2: // Wait before next scan cycle
    if (TiGetTics(TI_RFID) >= RFID_SCAN_DELAY)
    {
      rfid_reading_state = 0;
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

  // Transfer UID data byte by byte (cooperative approach)
  if (card_data_position < 5)
  {
    user_uid_buffer[card_data_position] = card_uid[card_data_position];
    card_data_position++;
    return FALSE;
  }

  // Transfer complete - reset for next reading
  rfid_card_detected = FALSE;
  card_data_position = 0;
  return TRUE;
}

void fake_rfid_read(void)
{
  rfid_card_detected = TRUE;
  card_data_position = 0;

  static BYTE user_index = 1;
  card_uid[0] = accepted_uids[user_index][0];
  card_uid[1] = accepted_uids[user_index][1];
  card_uid[2] = accepted_uids[user_index][2];
  card_uid[3] = accepted_uids[user_index][3];
  card_uid[4] = accepted_uids[user_index][4];
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

  // Send address byte
  for (i = 0; i < 8; i++)
  {
    MFRC522_SI = (addr & 0x80) ? 1 : 0;
    MFRC522_SCK = 1;
    addr <<= 1;
    MFRC522_SCK = 0;
  }

  // Read data byte
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

  // Send address byte
  for (i = 0; i < 8; i++)
  {
    MFRC522_SI = (addr & 0x80) ? 1 : 0;
    MFRC522_SCK = 1;
    addr <<= 1;
    MFRC522_SCK = 0;
  }

  // Send data byte
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

static void mfrc522_antenna_off(void)
{
  mfrc522_clear_register_bit(0x14, 0x03);
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
  mfrc522_antenna_off();
  mfrc522_antenna_on();
}

static BYTE mfrc522_anticollision_detection(BYTE *serial_number)
{
  BYTE status = MI_ERR;
  BYTE i, checksum = 0;
  WORD response_length;

  mfrc522_write_register(0x0D, 0x00);
  serial_number[0] = PICC_ANTICOLL;
  serial_number[1] = 0x20;
  mfrc522_clear_register_bit(0x08, 0x08);

  status = mfrc522_send_command_to_card(PCD_TRANSCEIVE, serial_number, 2, serial_number, &response_length);
  if (status == MI_OK)
  {
    // Verify checksum
    for (i = 0; i < 4; i++)
      checksum ^= serial_number[i];
    if (checksum != serial_number[4])
      status = MI_ERR;
  }
  return status;
}

static BYTE mfrc522_read_card_uid(BYTE *uid_buffer)
{
  BYTE status = mfrc522_anticollision_detection(uid_buffer);
  uid_buffer[5] = 0;
  return status == MI_OK;
}

static void mfrc522_halt_card_communication(void)
{
  WORD response_length;
  BYTE halt_command[4];
  halt_command[0] = PICC_HALT;
  halt_command[1] = 0;
  mfrc522_calculate_crc(halt_command, 2, &halt_command[2]);
  mfrc522_clear_register_bit(0x08, 0x80);
  mfrc522_send_command_to_card(PCD_TRANSCEIVE, halt_command, 4, halt_command, &response_length);
  mfrc522_clear_register_bit(0x08, 0x08);
}

static void mfrc522_calculate_crc(BYTE *data_in, BYTE length, BYTE *data_out)
{
  BYTE i, n;
  mfrc522_clear_register_bit(0x05, 0x04);
  mfrc522_set_register_bit(0x0A, 0x80);

  for (i = 0; i < length; i++)
    mfrc522_write_register(0x09, data_in[i]);
  mfrc522_write_register(0x01, PCD_CALCCRC);

  i = 0xFF;
  do
  {
    n = mfrc522_read_register(0x05);
    i--;
  } while (i && !(n & 0x04));

  data_out[0] = mfrc522_read_register(0x22);
  data_out[1] = mfrc522_read_register(0x21);
}

static BYTE mfrc522_send_command_to_card(BYTE command, BYTE *send_data, BYTE send_len, BYTE *back_data, WORD *back_len)
{
  BYTE status = MI_ERR;
  BYTE irq_enable = 0, wait_irq = 0;
  BYTE i, n, last_bits;
  WORD timeout_counter;

  if (command == 0x0E)
  {
    irq_enable = 0x12;
    wait_irq = 0x10;
  }
  else if (command == PCD_TRANSCEIVE)
  {
    irq_enable = 0x77;
    wait_irq = 0x30;
  }

  mfrc522_write_register(0x02, irq_enable | 0x80);
  mfrc522_clear_register_bit(0x04, 0x80);
  mfrc522_set_register_bit(0x0A, 0x80);
  mfrc522_write_register(0x01, PCD_IDLE);

  for (i = 0; i < send_len; i++)
    mfrc522_write_register(0x09, send_data[i]);

  mfrc522_write_register(0x01, command);
  if (command == PCD_TRANSCEIVE)
    mfrc522_set_register_bit(0x0D, 0x80);

  timeout_counter = 0xFFFF;
  do
  {
    n = mfrc522_read_register(0x04);
    timeout_counter--;
  } while (timeout_counter && !(n & 0x01) && !(n & wait_irq));

  mfrc522_clear_register_bit(0x0D, 0x80);

  if (timeout_counter && !(mfrc522_read_register(0x06) & 0x1B))
  {
    status = MI_OK;
    if (n & irq_enable & 0x01)
      status = MI_NOTAGERR;
    if (command == PCD_TRANSCEIVE)
    {
      n = mfrc522_read_register(0x0A);
      last_bits = mfrc522_read_register(0x0C) & 0x07;
      *back_len = last_bits ? ((n - 1) * 8 + last_bits) : (n * 8);
      if (n == 0)
        n = 1;
      else if (n > 16)
        n = 16;
      for (i = 0; i < n; i++)
        back_data[i] = mfrc522_read_register(0x09);
      back_data[i] = 0;
    }
  }
  return status;
}
