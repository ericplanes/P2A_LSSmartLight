#include "TRFID.h"
#include "TTimer.h"

/* =======================================
 *         REGISTER ADDRESSES
 * ======================================= */

// Page 0: Command and Status
#define REG_CMD 0x01
#define REG_IRQ_EN 0x02
#define REG_IRQ 0x04
#define REG_ERR 0x06
#define REG_FIFO_DATA 0x09
#define REG_FIFO_LEVEL 0x0A
#define REG_BIT_FRAME 0x0D

// Page 1: Communication
#define REG_TX_CTRL 0x14

/* =======================================
 *         COMMANDS
 * ======================================= */

#define CMD_IDLE 0x00
#define CMD_TRANSCEIVE 0x0C

/* =======================================
 *         CARD COMMANDS
 * ======================================= */

#define CARD_REQ 0x26
#define CARD_ANTICOLL 0x93
#define CARD_HALT 0x50

/* =======================================
 *         STATES
 * ======================================= */

// Main states
#define STATE_IDLE 0
#define STATE_INIT 1
#define STATE_SEND_REQ 2
#define STATE_WAIT_RESP 3
#define STATE_ANTICOLL_START 4
#define STATE_ANTICOLL_WAIT 5
#define STATE_ANTICOLL_READ 6
#define STATE_HALT 7
#define STATE_DELAY 8

// SPI states
#define SPI_IDLE 0
#define SPI_SEND_ADDR 1
#define SPI_SEND_DATA 2
#define SPI_READ_DATA 3
#define SPI_COMPLETE 4

// Communication states
#define COMM_IDLE 0
#define COMM_SETUP 1
#define COMM_SEND 2
#define COMM_WAIT 3
#define COMM_READ 4

/* =======================================
 *         CONSTANTS
 * ======================================= */

#define SPI_BITS 8
#define SPI_READ_MASK 0x80
#define SPI_WRITE_MASK 0x7E
#define SPI_ADDR_SHIFT 1

#define TIMEOUT_RETRIES 0xFF
#define IRQ_RETRIES 0x0F
#define DELAY_MS 300

#define FIFO_SIZE 16
#define UID_BYTES 4
#define UID_SIZE 5

#define IRQ_MASK_01 0x01
#define IRQ_MASK_30 0x30

/* =======================================
 *         PRIVATE VARIABLES
 * ======================================= */

static BYTE state;
static BYTE timer_handle;
static BOOL card_detected;
static BYTE uid[RFID_UID_SIZE];
static BYTE uid_pos;

// SPI variables
static BYTE spi_state;
static BYTE spi_bit_count;
static BYTE spi_addr;
static BYTE spi_data;
static BYTE spi_result;
static BOOL spi_read;

// Communication variables
static BYTE comm_state;
static BYTE comm_cmd;
static BYTE comm_retry;
static BYTE comm_buffer[16];
static BYTE comm_length;
static BYTE comm_resp_length;

// Anticollision variables
static BYTE anticoll_buffer[5];
static BYTE anticoll_checksum;
static BYTE anticoll_index;

/* =======================================
 *       PRIVATE FUNCTION HEADERS
 * ======================================= */

static void config_pins(void);
static void reset_chip(void);
static void init_chip(void);
static BOOL spi_motor(BYTE reg, BYTE data, BOOL read);
static BOOL comm_motor(BYTE cmd, BYTE *send_data, BYTE send_len);
static void start_spi(BYTE reg, BYTE data, BOOL read);
static void start_comm(BYTE cmd, BYTE *send_data, BYTE send_len);

/* =======================================
 *         PUBLIC FUNCTION BODIES
 * ======================================= */

void RFID_Init(void)
{
  config_pins();
  reset_chip();
  state = STATE_INIT;
  card_detected = FALSE;
  uid_pos = 0;
  spi_state = SPI_IDLE;
  comm_state = COMM_IDLE;
  TiNewTimer(&timer_handle);
  TiResetTics(timer_handle);
}

void RFID_Motor(void)
{
  switch (state)
  {
  case STATE_IDLE:
    TiResetTics(timer_handle);
    state = STATE_DELAY;
    break;

  case STATE_INIT:
    if (spi_motor(REG_BIT_FRAME, 0x07, FALSE))
    {
      state = STATE_SEND_REQ;
    }
    break;

  case STATE_SEND_REQ:
    comm_buffer[0] = CARD_REQ;
    start_comm(CMD_TRANSCEIVE, comm_buffer, 1);
    state = STATE_WAIT_RESP;
    break;

  case STATE_WAIT_RESP:
    if (comm_motor(CMD_TRANSCEIVE, comm_buffer, 1))
    {
      state = STATE_IDLE;
      if (comm_resp_length > 0)
      {
        state = STATE_ANTICOLL_START;
      }
    }
    break;

  case STATE_ANTICOLL_START:
    anticoll_buffer[0] = CARD_ANTICOLL;
    anticoll_buffer[1] = 0x20;
    anticoll_checksum = 0;
    anticoll_index = 0;
    start_comm(CMD_TRANSCEIVE, anticoll_buffer, 2);
    state = STATE_ANTICOLL_WAIT;
    break;

  case STATE_ANTICOLL_WAIT:
    if (comm_motor(CMD_TRANSCEIVE, anticoll_buffer, 2))
    {
      state = STATE_IDLE;
      if (comm_resp_length >= 5)
      {
        state = STATE_ANTICOLL_READ;
      }
    }
    break;

  case STATE_ANTICOLL_READ:
    if (anticoll_index < UID_BYTES)
    {
      anticoll_checksum ^= comm_buffer[anticoll_index];
      uid[anticoll_index] = comm_buffer[anticoll_index];
      anticoll_index++;
    }
    if (anticoll_index >= UID_BYTES)
    {
      if (anticoll_checksum == comm_buffer[UID_BYTES])
      {
        card_detected = TRUE;
        uid_pos = 0;
      }
      state = STATE_HALT;
    }
    break;

  case STATE_HALT:
    comm_buffer[0] = CARD_HALT;
    comm_buffer[1] = 0;
    start_comm(CMD_TRANSCEIVE, comm_buffer, 2);
    state = STATE_IDLE;
    break;

  case STATE_DELAY:
    if (TiGetTics(timer_handle) >= DELAY_MS)
    {
      state = STATE_INIT;
    }
    break;

  default:
    state = STATE_IDLE;
    break;
  }
}

BOOL RFID_HasReadUser(void)
{
  return card_detected;
}

BOOL RFID_GetReadUserId(BYTE *user_uid_buffer)
{
  if (!card_detected)
    return FALSE;

  if (uid_pos < RFID_UID_SIZE)
  {
    user_uid_buffer[uid_pos] = uid[uid_pos];
    uid_pos++;
    return FALSE;
  }

  // Transfer complete
  card_detected = FALSE;
  uid_pos = 0;
  return TRUE;
}

/* =======================================
 *        PRIVATE FUNCTION BODIES
 * ======================================= */

static void config_pins(void)
{
  DIR_MFRC522_SO = 1;  // Input (MISO)
  DIR_MFRC522_SI = 0;  // Output (MOSI)
  DIR_MFRC522_SCK = 0; // Output (SCK)
  DIR_MFRC522_CS = 0;  // Output (CS)
  DIR_MFRC522_RST = 0; // Output (RST)
}

static void reset_chip(void)
{
  MFRC522_RST = 1;
  MFRC522_RST = 0;
  MFRC522_RST = 1;
}

static void init_chip(void)
{
  MFRC522_CS = 1;
  MFRC522_RST = 1;
  reset_chip();
}

static BOOL spi_motor(BYTE reg, BYTE data, BOOL read)
{
  switch (spi_state)
  {
  case SPI_IDLE:
    start_spi(reg, data, read);
    spi_state = SPI_SEND_ADDR;
    spi_bit_count = 0;
    MFRC522_SCK = 0;
    MFRC522_CS = 0;
    return FALSE;

  case SPI_SEND_ADDR:
    MFRC522_SI = (spi_addr & 0x80) ? 1 : 0;
    MFRC522_SCK = 1;
    spi_addr <<= 1;
    MFRC522_SCK = 0;
    spi_bit_count++;

    if (spi_bit_count >= SPI_BITS)
    {
      spi_bit_count = 0;
      spi_state = read ? SPI_READ_DATA : SPI_SEND_DATA;
      if (read)
        spi_result = 0;
    }
    return FALSE;

  case SPI_SEND_DATA:
    MFRC522_SI = (spi_data & 0x80) ? 1 : 0;
    MFRC522_SCK = 1;
    spi_data <<= 1;
    MFRC522_SCK = 0;
    spi_bit_count++;

    if (spi_bit_count >= SPI_BITS)
    {
      spi_state = SPI_COMPLETE;
    }
    return FALSE;

  case SPI_READ_DATA:
    MFRC522_SCK = 1;
    spi_result = (BYTE)((spi_result << 1) | (MFRC522_SO ? 1 : 0));
    MFRC522_SCK = 0;
    spi_bit_count++;

    if (spi_bit_count >= SPI_BITS)
    {
      spi_state = SPI_COMPLETE;
    }
    return FALSE;

  case SPI_COMPLETE:
    MFRC522_CS = 1;
    MFRC522_SCK = 1;
    spi_state = SPI_IDLE;
    return TRUE;

  default:
    spi_state = SPI_IDLE;
    return FALSE;
  }
}

static BOOL comm_motor(BYTE cmd, BYTE *send_data, BYTE send_len)
{
  static BYTE byte_idx;
  static BYTE irq_val;

  switch (comm_state)
  {
  case COMM_IDLE:
    return TRUE;

  case COMM_SETUP:
    if (spi_motor(REG_IRQ_EN, 0x77 | 0x80, FALSE))
    {
      comm_state = COMM_SEND;
      byte_idx = 0;
    }
    return FALSE;

  case COMM_SEND:
    if (byte_idx < comm_length && spi_motor(REG_FIFO_DATA, comm_buffer[byte_idx], FALSE))
    {
      byte_idx++;
    }
    if (byte_idx >= comm_length && spi_motor(REG_CMD, comm_cmd, FALSE))
    {
      comm_retry = IRQ_RETRIES;
      comm_state = COMM_WAIT;
    }
    return FALSE;

  case COMM_WAIT:
    if (spi_motor(REG_IRQ, 0, TRUE))
    {
      irq_val = spi_result;
      if ((irq_val & IRQ_MASK_01) || (irq_val & IRQ_MASK_30) || (comm_retry == 0))
      {
        comm_state = COMM_READ;
        byte_idx = 0;
      }
      else if (comm_retry > 0)
      {
        comm_retry--;
      }
    }
    return FALSE;

  case COMM_READ:
    if (spi_motor(REG_FIFO_LEVEL, 0, TRUE))
    {
      comm_resp_length = spi_result;
      if (comm_resp_length > 0 && comm_resp_length <= 16)
      {
        if (byte_idx < comm_resp_length && spi_motor(REG_FIFO_DATA, 0, TRUE))
        {
          comm_buffer[byte_idx] = spi_result;
          byte_idx++;
        }
        else
        {
          comm_state = COMM_IDLE;
          return TRUE;
        }
      }
      if (comm_resp_length == 0 || comm_resp_length > 16)
      {
        comm_resp_length = 0;
        comm_state = COMM_IDLE;
        return TRUE;
      }
    }
    return FALSE;

  default:
    comm_state = COMM_IDLE;
    return TRUE;
  }
}

static void start_spi(BYTE reg, BYTE data, BOOL read)
{
  spi_read = read;
  spi_data = data;

  if (read)
  {
    spi_addr = ((reg << SPI_ADDR_SHIFT) & SPI_WRITE_MASK) | SPI_READ_MASK;
  }
  else
  {
    spi_addr = ((reg << SPI_ADDR_SHIFT) & SPI_WRITE_MASK);
  }
}

static void start_comm(BYTE cmd, BYTE *send_data, BYTE send_len)
{
  BYTE i;

  comm_cmd = cmd;
  comm_length = send_len;
  comm_state = COMM_SETUP;

  for (i = 0; i < send_len && i < 16; i++)
  {
    comm_buffer[i] = send_data[i];
  }
}
