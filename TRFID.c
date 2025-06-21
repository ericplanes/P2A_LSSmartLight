#include "TRFID.h"
#include "TTimer.h"
#include "TUserControl.h"

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
#define UID_BYTES 5
#define UID_SIZE 5

#define IRQ_MASK_01 0x01
#define IRQ_MASK_30 0x30

/* =======================================
 *         PRIVATE VARIABLES
 * ======================================= */

static BOOL card_detected;
static const BYTE *uid;
static BYTE uid_pos;

/* =======================================
 *         PUBLIC FUNCTION BODIES
 * ======================================= */

void RFID_Init(void)
{
  card_detected = TRUE;
  uid_pos = 0;
  uid = accepted_uids[1];
}

void RFID_Motor(void)
{
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
