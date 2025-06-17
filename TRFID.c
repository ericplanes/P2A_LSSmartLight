#include "TRFID.h"
#include "TTimer.h"

/* =======================================
 *              CONSTANTS
 * ======================================= */

// Motor states
#define STATE_RFID_INIT 0
#define STATE_RFID_WAIT_IRQ 1
#define STATE_RFID_CHECK_RESPONSE 2
#define STATE_RFID_READ_CARD 3
#define STATE_RFID_CONFIG_FRAME 4
#define STATE_RFID_CONFIG_IRQ 5
#define STATE_RFID_CLEAR_IRQ 6
#define STATE_RFID_SET_IDLE 7
#define STATE_RFID_SEND_REQ 8
#define STATE_RFID_START_TRANSCEIVE 9
#define STATE_RFID_START_SEND 10
#define STATE_RFID_CHECK_STATUS 11
#define STATE_RFID_READ_FIFO_LEVEL 12
#define STATE_RFID_CALC_BITS 13
#define STATE_RFID_CHECK_SIZE 14
#define STATE_RFID_READ_FIFO 15
#define STATE_RFID_PROCESS_DATA 16
#define STATE_RFID_WAIT_TIMEOUT 17

// SPI communication timing
#define SPI_BITS_PER_BYTE 8
#define SPI_READ_MASK 0x80  // MSB=1 for read operations
#define SPI_WRITE_MASK 0x7E // Address mask for write operations
#define SPI_ADDRESS_SHIFT 1 // Left shift for address

// MFRC522 communication timeouts and thresholds
#define TIMEOUT_MAX_RETRIES 0x1FF     // Reduced from 65535 to 511
#define IRQ_TIMEOUT_RETRIES 0x07      // Reduced from 15 to 7
#define ANTICOLL_TIMEOUT_RETRIES 0x3F // Reduced from 255 to 63
#define CARD_DETECTION_DELAY_MS 500

// FIFO and data handling
#define FIFO_MAX_SIZE 16
#define MIN_FIFO_SIZE 1
#define UID_BYTES_COUNT 4
#define UID_WITH_CHECKSUM_SIZE 5

// Error checking masks
#define ERROR_REGISTER_MASK 0x1B    // Bits 0,1,3,4 of ErrorReg
#define IRQ_CHECK_MASK_01 0x01      // Bit 0 of IRQ register
#define IRQ_CHECK_MASK_30 0x30      // Bits 4,5 of IRQ register
#define CONTROL_LAST_BITS_MASK 0x07 // Last 3 bits of ControlReg

// Antenna and RF configuration
#define ANTENNA_ENABLE_MASK 0x03 // Enable TX1 and TX2
#define CRC_IRQ_BIT_MASK 0x04    // CRC interrupt bit

/* =======================================
 *         PRIVATE VARIABLES
 * ======================================= */

static BYTE current_rfid_state;
static BYTE communication_status;
static BYTE rfid_timer_handle;
static BOOL user_card_detected;
static BYTE stored_user_uid[RFID_UID_SIZE];
static BYTE uid_transfer_position;

/* =======================================
 *       PRIVATE FUNCTION HEADERS
 * ======================================= */

static void configure_rfid_pins(void);
static BYTE read_mfrc522_register(BYTE register_address);
static void write_mfrc522_register(BYTE register_address, BYTE register_value);
static void clear_register_bits(BYTE register_address, BYTE bit_mask);
static void set_register_bits(BYTE register_address, BYTE bit_mask);
static void reset_mfrc522_chip(void);
static void enable_antenna(void);
static void disable_antenna(void);
static void initialize_mfrc522_chip(void);
static BYTE communicate_with_card(BYTE command, BYTE *send_data, BYTE send_length, BYTE *receive_data, WORD *receive_length);
static void calculate_crc_checksum(BYTE *input_data, BYTE data_length, BYTE *crc_output);
static BYTE perform_anticollision(BYTE *serial_number);
static BYTE read_card_serial_number(BYTE *uid_string);
static void halt_card_communication(void);
static void store_detected_uid(BYTE *detected_uid);

/* =======================================
 *         PUBLIC FUNCTION BODIES
 * ======================================= */

void RFID_Init(void)
{
  configure_rfid_pins();
  initialize_mfrc522_chip();
  current_rfid_state = STATE_RFID_INIT;
  user_card_detected = FALSE;
  uid_transfer_position = 0;
  TiNewTimer(&rfid_timer_handle);
  TiResetTics(rfid_timer_handle);
}

void RFID_Motor(void)
{
  BYTE detected_uid[UID_WITH_CHECKSUM_SIZE];
  static BYTE irq_retry_counter;
  BYTE irq_register_value, last_received_bits;
  WORD calculated_back_bits;

  switch (current_rfid_state)
  {
  case STATE_RFID_INIT:
    irq_retry_counter = IRQ_TIMEOUT_RETRIES;
    current_rfid_state = STATE_RFID_CONFIG_FRAME;
    break;

  case STATE_RFID_CONFIG_FRAME:
    // Combined configuration sequence to save memory
    write_mfrc522_register(BITFRAMINGREG, 0x07);
    write_mfrc522_register(COMMIENREG, 0x77 | 0x80);
    clear_register_bits(COMMIRQREG, 0x80);
    set_register_bits(FIFOLEVELREG, 0x80);
    write_mfrc522_register(COMMANDREG, PCD_IDLE);
    current_rfid_state = STATE_RFID_SEND_REQ;
    break;

  case STATE_RFID_SEND_REQ:
    // Combined request sequence to save memory
    write_mfrc522_register(FIFODATAREG, PICC_REQIDL);
    write_mfrc522_register(COMMANDREG, PCD_TRANSCEIVE);
    set_register_bits(BITFRAMINGREG, 0x80);
    current_rfid_state = STATE_RFID_WAIT_IRQ;
    break;

  case STATE_RFID_WAIT_IRQ:
    irq_register_value = read_mfrc522_register(COMMIRQREG);
    if (irq_retry_counter && !(irq_register_value & IRQ_CHECK_MASK_01) && !(irq_register_value & IRQ_CHECK_MASK_30))
      irq_retry_counter--;
    else
      current_rfid_state = STATE_RFID_CHECK_RESPONSE;
    break;

  case STATE_RFID_CHECK_RESPONSE:
    clear_register_bits(BITFRAMINGREG, 0x80);
    if (irq_retry_counter != 0 && !(read_mfrc522_register(ERRORREG) & ERROR_REGISTER_MASK))
      current_rfid_state = STATE_RFID_CHECK_STATUS;
    else
      current_rfid_state = STATE_RFID_INIT;
    break;

  case STATE_RFID_CHECK_STATUS:
    communication_status = MI_OK;
    if (irq_register_value & 0x77 & IRQ_CHECK_MASK_01)
      communication_status = MI_NOTAGERR;
    current_rfid_state = STATE_RFID_READ_FIFO_LEVEL;
    break;

  case STATE_RFID_READ_FIFO_LEVEL:
    irq_register_value = read_mfrc522_register(FIFOLEVELREG);
    current_rfid_state = STATE_RFID_CALC_BITS;
    break;

  case STATE_RFID_CALC_BITS:
    last_received_bits = read_mfrc522_register(CONTROLREG) & CONTROL_LAST_BITS_MASK;
    calculated_back_bits = last_received_bits ? ((irq_register_value - 1) * SPI_BITS_PER_BYTE + last_received_bits) : (irq_register_value * SPI_BITS_PER_BYTE);
    current_rfid_state = STATE_RFID_CHECK_SIZE;
    break;

  case STATE_RFID_CHECK_SIZE:
    if (irq_register_value == 0)
      irq_register_value = MIN_FIFO_SIZE;
    else if (irq_register_value > FIFO_MAX_SIZE)
      irq_register_value = FIFO_MAX_SIZE;
    current_rfid_state = STATE_RFID_READ_FIFO;
    break;

  case STATE_RFID_READ_FIFO:
    (void)read_mfrc522_register(FIFODATAREG);
    current_rfid_state = STATE_RFID_PROCESS_DATA;
    break;

  case STATE_RFID_PROCESS_DATA:
    current_rfid_state = STATE_RFID_READ_CARD;
    break;

  case STATE_RFID_READ_CARD:
    if (read_card_serial_number(detected_uid))
    {
      store_detected_uid(detected_uid);
    }
    halt_card_communication();
    TiResetTics(rfid_timer_handle);
    current_rfid_state = STATE_RFID_WAIT_TIMEOUT;
    break;

  case STATE_RFID_WAIT_TIMEOUT:
    if (TiGetTics(rfid_timer_handle) >= CARD_DETECTION_DELAY_MS)
      current_rfid_state = STATE_RFID_INIT;
    break;

  default:
    current_rfid_state = STATE_RFID_INIT;
    break;
  }
}

BOOL RFID_HasReadUser(void)
{
  return user_card_detected;
}

BOOL RFID_GetReadUserId(BYTE *user_uid_buffer)
{
  if (!user_card_detected)
    return FALSE;

  if (uid_transfer_position < RFID_UID_SIZE)
  {
    user_uid_buffer[uid_transfer_position] = stored_user_uid[uid_transfer_position];
    uid_transfer_position++;
    return FALSE;
  }
  else
  {
    // Transfer complete, reset flags for next reading
    user_card_detected = FALSE;
    uid_transfer_position = 0;
    return TRUE;
  }
}

/* =======================================
 *        PRIVATE FUNCTION BODIES
 * ======================================= */

static void configure_rfid_pins(void)
{
  DIR_MFRC522_SO = 1;  // Input (MISO)
  DIR_MFRC522_SI = 0;  // Output (MOSI)
  DIR_MFRC522_SCK = 0; // Output (SCK)
  DIR_MFRC522_CS = 0;  // Output (CS)
  DIR_MFRC522_RST = 0; // Output (RST)
}

static BYTE read_mfrc522_register(BYTE register_address)
{
  BYTE bit_counter, read_result = 0;
  BYTE spi_address = ((register_address << SPI_ADDRESS_SHIFT) & SPI_WRITE_MASK) | SPI_READ_MASK;

  MFRC522_SCK = 0;
  MFRC522_CS = 0;

  // Send address byte
  for (bit_counter = 0; bit_counter < SPI_BITS_PER_BYTE; bit_counter++)
  {
    MFRC522_SI = (spi_address & 0x80) ? 1 : 0;
    MFRC522_SCK = 1;
    spi_address <<= 1;
    MFRC522_SCK = 0;
  }

  // Read data byte
  for (bit_counter = 0; bit_counter < SPI_BITS_PER_BYTE; bit_counter++)
  {
    MFRC522_SCK = 1;
    read_result = (BYTE)((read_result << 1) | (MFRC522_SO ? 1 : 0));
    MFRC522_SCK = 0;
  }

  MFRC522_CS = 1;
  MFRC522_SCK = 1;
  return read_result;
}

static void write_mfrc522_register(BYTE register_address, BYTE register_value)
{
  BYTE bit_counter;
  BYTE spi_address = ((register_address << SPI_ADDRESS_SHIFT) & SPI_WRITE_MASK);

  MFRC522_SCK = 0;
  MFRC522_CS = 0;

  // Send address byte
  for (bit_counter = 0; bit_counter < SPI_BITS_PER_BYTE; bit_counter++)
  {
    MFRC522_SI = (spi_address & 0x80) ? 1 : 0;
    MFRC522_SCK = 1;
    spi_address <<= 1;
    MFRC522_SCK = 0;
  }

  // Send data byte
  for (bit_counter = 0; bit_counter < SPI_BITS_PER_BYTE; bit_counter++)
  {
    MFRC522_SI = (register_value & 0x80) ? 1 : 0;
    MFRC522_SCK = 1;
    register_value <<= 1;
    MFRC522_SCK = 0;
  }

  MFRC522_CS = 1;
  MFRC522_SCK = 1;
}

static void clear_register_bits(BYTE register_address, BYTE bit_mask)
{
  BYTE current_register_value = read_mfrc522_register(register_address);
  write_mfrc522_register(register_address, current_register_value & ~bit_mask);
}

static void set_register_bits(BYTE register_address, BYTE bit_mask)
{
  BYTE current_register_value = read_mfrc522_register(register_address);
  write_mfrc522_register(register_address, current_register_value | bit_mask);
}

static void reset_mfrc522_chip(void)
{
  MFRC522_RST = 1;
  MFRC522_RST = 0;
  MFRC522_RST = 1;
  write_mfrc522_register(COMMANDREG, PCD_RESETPHASE);
}

static void enable_antenna(void)
{
  set_register_bits(TXCONTROLREG, ANTENNA_ENABLE_MASK);
}

static void disable_antenna(void)
{
  clear_register_bits(TXCONTROLREG, ANTENNA_ENABLE_MASK);
}

static void initialize_mfrc522_chip(void)
{
  MFRC522_CS = 1;
  MFRC522_RST = 1;
  reset_mfrc522_chip();

  // Timer configuration for optimal RFID operation
  write_mfrc522_register(TMODEREG, 0x8D);      // TAuto=1; f(Timer) = 6.78MHz/TPrescaler
  write_mfrc522_register(TPRESCALERREG, 0x3E); // TModeReg[3..0] + TPrescalerReg
  write_mfrc522_register(TRELOADREGL, 30);     // Reload value low byte
  write_mfrc522_register(TRELOADREGH, 0);      // Reload value high byte
  write_mfrc522_register(TXAUTOREG, 0x40);     // 100% ASK modulation
  write_mfrc522_register(MODEREG, 0x3D);       // CRC initial value 0x6363

  disable_antenna();
  enable_antenna();
}

static BYTE communicate_with_card(BYTE command, BYTE *send_data, BYTE send_length, BYTE *receive_data, WORD *receive_length)
{
  BYTE operation_status = MI_ERR;
  BYTE interrupt_enable = 0, wait_interrupt = 0;
  BYTE byte_counter, irq_value, last_valid_bits;
  WORD timeout_counter;

  if (command == PCD_AUTHENT)
  {
    interrupt_enable = 0x12;
    wait_interrupt = 0x10;
  }
  else if (command == PCD_TRANSCEIVE)
  {
    interrupt_enable = 0x77;
    wait_interrupt = 0x30;
  }

  write_mfrc522_register(COMMIENREG, interrupt_enable | 0x80);
  clear_register_bits(COMMIRQREG, 0x80);
  set_register_bits(FIFOLEVELREG, 0x80);
  write_mfrc522_register(COMMANDREG, PCD_IDLE);

  for (byte_counter = 0; byte_counter < send_length; byte_counter++)
  {
    write_mfrc522_register(FIFODATAREG, send_data[byte_counter]);
  }

  write_mfrc522_register(COMMANDREG, command);
  if (command == PCD_TRANSCEIVE)
    set_register_bits(BITFRAMINGREG, 0x80);

  timeout_counter = TIMEOUT_MAX_RETRIES;
  do
  {
    irq_value = read_mfrc522_register(COMMIRQREG);
    timeout_counter--;
  } while (timeout_counter && !(irq_value & IRQ_CHECK_MASK_01) && !(irq_value & wait_interrupt));

  clear_register_bits(BITFRAMINGREG, 0x80);

  if (timeout_counter != 0)
  {
    if (!(read_mfrc522_register(ERRORREG) & ERROR_REGISTER_MASK))
    {
      operation_status = MI_OK;
      if (irq_value & interrupt_enable & IRQ_CHECK_MASK_01)
        operation_status = MI_NOTAGERR;

      if (command == PCD_TRANSCEIVE)
      {
        irq_value = read_mfrc522_register(FIFOLEVELREG);
        last_valid_bits = read_mfrc522_register(CONTROLREG) & CONTROL_LAST_BITS_MASK;
        if (last_valid_bits)
          *receive_length = (irq_value - 1) * SPI_BITS_PER_BYTE + last_valid_bits;
        else
          *receive_length = irq_value * SPI_BITS_PER_BYTE;

        if (irq_value == 0)
          irq_value = MIN_FIFO_SIZE;
        else if (irq_value > FIFO_MAX_SIZE)
          irq_value = FIFO_MAX_SIZE;

        for (byte_counter = 0; byte_counter < irq_value; byte_counter++)
        {
          receive_data[byte_counter] = read_mfrc522_register(FIFODATAREG);
        }
        receive_data[byte_counter] = 0;
      }
    }
    else
    {
      operation_status = MI_ERR;
    }
  }
  return operation_status;
}

static void calculate_crc_checksum(BYTE *input_data, BYTE data_length, BYTE *crc_output)
{
  BYTE byte_counter, crc_status;
  clear_register_bits(DIVIRQREG, CRC_IRQ_BIT_MASK);
  set_register_bits(FIFOLEVELREG, 0x80);

  for (byte_counter = 0; byte_counter < data_length; byte_counter++)
  {
    write_mfrc522_register(FIFODATAREG, input_data[byte_counter]);
  }

  write_mfrc522_register(COMMANDREG, PCD_CALCCRC);

  byte_counter = ANTICOLL_TIMEOUT_RETRIES;
  do
  {
    crc_status = read_mfrc522_register(DIVIRQREG);
    byte_counter--;
  } while (byte_counter && !(crc_status & CRC_IRQ_BIT_MASK));

  crc_output[0] = read_mfrc522_register(CRCRESULTREGL);
  crc_output[1] = read_mfrc522_register(CRCRESULTREGM);
}

static BYTE perform_anticollision(BYTE *serial_number)
{
  BYTE anticoll_status = MI_ERR;
  BYTE byte_counter, checksum_verification = 0;
  WORD response_length;

  write_mfrc522_register(BITFRAMINGREG, 0x00);
  serial_number[0] = PICC_ANTICOLL;
  serial_number[1] = 0x20;
  clear_register_bits(STATUS2REG, 0x08);
  anticoll_status = communicate_with_card(PCD_TRANSCEIVE, serial_number, 2, serial_number, &response_length);

  if (anticoll_status == MI_OK)
  {
    for (byte_counter = 0; byte_counter < UID_BYTES_COUNT; byte_counter++)
    {
      checksum_verification ^= serial_number[byte_counter];
    }
    if (checksum_verification != serial_number[UID_BYTES_COUNT])
      anticoll_status = MI_ERR;
  }
  return anticoll_status;
}

static BYTE read_card_serial_number(BYTE *uid_string)
{
  BYTE read_operation_status = perform_anticollision(uid_string);
  uid_string[UID_WITH_CHECKSUM_SIZE] = 0; // Null terminate
  return (read_operation_status == MI_OK) ? 1 : 0;
}

static void halt_card_communication(void)
{
  WORD halt_response_length;
  BYTE halt_command_buffer[4];
  halt_command_buffer[0] = PICC_HALT;
  halt_command_buffer[1] = 0;
  calculate_crc_checksum(halt_command_buffer, 2, &halt_command_buffer[2]);
  clear_register_bits(STATUS2REG, 0x80);
  communicate_with_card(PCD_TRANSCEIVE, halt_command_buffer, 4, halt_command_buffer, &halt_response_length);
  clear_register_bits(STATUS2REG, 0x08);
}

static void store_detected_uid(BYTE *detected_uid)
{
  BYTE uid_byte_index;
  for (uid_byte_index = 0; uid_byte_index < RFID_UID_SIZE; uid_byte_index++)
  {
    stored_user_uid[uid_byte_index] = detected_uid[uid_byte_index];
  }
  user_card_detected = TRUE;
  uid_transfer_position = 0;
}
