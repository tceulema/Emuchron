//*****************************************************************************
// Filename : 'i2c.c'
// Title    : I2C interface using AVR Two-Wire Interface (TWI) hardware
//*****************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include "util.h"
#include "i2cconf.h"
#include "i2c.h"

// Standard I2C bit rates are:
// 100KHz for slow speed
// 400KHz for high speed

// Generate i2c debug strings.
// 0 = Off, 1 = On
// Note: Actually putting the strings onto the FTDI bus requires the master
// debug flag in monomain.h [firmware] to be enabled.
#define I2C_DEBUG	0
#define DEBUGI(x)	if (I2C_DEBUG) { x; }
#define DEBUGIP(x)	DEBUGI(putstring_nl(x))

// TWSR values (not bits)
// (taken from avr-libc twi.h - thank you Marek Michalkiewicz)
// Master
#define TW_START			0x08
#define TW_REP_START			0x10
// Master Transmitter
#define TW_MT_SLA_ACK			0x18
#define TW_MT_SLA_NACK			0x20
#define TW_MT_DATA_ACK			0x28
#define TW_MT_DATA_NACK			0x30
#define TW_MT_ARB_LOST			0x38
// Master Receiver
#define TW_MR_ARB_LOST			0x38
#define TW_MR_SLA_ACK			0x40
#define TW_MR_SLA_NACK			0x48
#define TW_MR_DATA_ACK			0x50
#define TW_MR_DATA_NACK			0x58
// Slave Transmitter
#define TW_ST_SLA_ACK			0xa8
#define TW_ST_ARB_LOST_SLA_ACK		0xb0
#define TW_ST_DATA_ACK			0xb8
#define TW_ST_DATA_NACK			0xc0
#define TW_ST_LAST_DATA			0xc8
// Slave Receiver
#define TW_SR_SLA_ACK			0x60
#define TW_SR_ARB_LOST_SLA_ACK		0x68
#define TW_SR_GCALL_ACK			0x70
#define TW_SR_ARB_LOST_GCALL_ACK	0x78
#define TW_SR_DATA_ACK			0x80
#define TW_SR_DATA_NACK			0x88
#define TW_SR_GCALL_DATA_ACK		0x90
#define TW_SR_GCALL_DATA_NACK		0x98
#define TW_SR_STOP			0xa0
// Misc status
#define TW_NO_INFO			0xf8
#define TW_BUS_ERROR			0x00

// Command and status processing
#define TWCR_CMD_MASK			0x0f
#define TWSR_STATUS_MASK		0xf8

// I2C state and address variables
static volatile eI2cStateType I2cState;
static u08 I2cDeviceAddrRW;
// Send/transmit buffer (outgoing data)
static u08 I2cSendData[I2C_SEND_DATA_BUFFER_SIZE];
static u08 I2cSendDataIndex;
static u08 I2cSendDataLength;
// Receive buffer (incoming data)
static u08 I2cReceiveData[I2C_RECEIVE_DATA_BUFFER_SIZE];
static u08 I2cReceiveDataIndex;
static u08 I2cReceiveDataLength;

// Function pointer to i2c receive/transmit routine.
// I2cSlaveReceive is called when this processor is addressed as a slave for
// writing.
// I2cSlaveTransmit is called when this processor is addressed as a slave for
// reading.
static void (*i2cSlaveReceive)(u08 receiveDataLength, u08* receiveData);
static u08 (*i2cSlaveTransmit)(u08 transmitDataLengthMax, u08* transmitData);

// Set the I2C transaction bitrate (in KHz)
static void i2cSetBitrate(u16 bitrateKHz);

// Low-level I2C transaction commands:
// Send an I2C start condition in Master mode
static void i2cSendStart(void);
// Send an I2C stop condition in Master mode
static void i2cSendStop(void);
// Wait for current I2C operation to complete
static void i2cWaitForComplete(void);
// Send an (address|R/W) combination or a data byte over I2C
static void i2cSendByte(u08 data);
// Receive a data byte over i2c:
// ackFlag = TRUE if received data should be ACK'ed
// ackFlag = FALSE if received data should be NACK'ed
static void i2cReceiveByte(u08 ackFlag);
// Pick up the data that was received with i2cReceiveByte()
static u08 i2cGetReceivedByte(void);
// Get current I2c bus status from TWSR
//static u08 i2cGetStatus(void);

// functions
void i2cInit(void)
{
  // Set pull-up resistors on I2C bus pins
  // TODO: should #ifdef these
  sbi(PORTC, 5);  // i2c SCL on ATmegaxx8
  sbi(PORTC, 4);  // i2c SDA on ATmegaxx8

  // Clear SlaveReceive and SlaveTransmit handler to null
  i2cSlaveReceive = 0;
  i2cSlaveTransmit = 0;
  // Set i2c bit rate to 100KHz
  i2cSetBitrate(100);
  // Enable TWI (two-wire interface)
  sbi(TWCR, TWEN);
  // Set state
  I2cState = I2C_IDLE;
  // Enable TWI interrupt and slave address ACK
  sbi(TWCR, TWIE);
  sbi(TWCR, TWEA);
  //outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT) | BV(TWEA));
  // Enable interrupts
  sei();
}

static void i2cSetBitrate(u16 bitrate)
{
  //u08 bitrate_div;
  // set i2c bitrate
  // SCL freq = F_CPU / (16 + 2 * TWBR)
  /*
  #ifdef TWPS0
  // For processors with additional bitrate division (mega128)
  // SCL freq = F_CPU / (16 + 2 * TWBR * 4 ^ TWPS)
  // set TWPS to zero
  cbi(TWSR, TWPS0);
  cbi(TWSR, TWPS1);
  #endif
  */

  // Calculate bitrate division
  //bitrate_div = (F_CPU / 32) / bitrate;
  //outb(TWBR, bitrate_div);
  TWBR = 32;
}

/*void i2cSetLocalDeviceAddr(u08 deviceAddr, u08 genCallEn)
{
  // Set local device address (used in slave mode only)
  outb(TWAR, ((deviceAddr & 0xfe) | (genCallEn ? 1 : 0)));
}*/

/*void i2cSetSlaveReceiveHandler(void (*i2cSlaveRx_func)
  (u08 receiveDataLength, u08* receiveData))
{
  i2cSlaveReceive = i2cSlaveRx_func;
}*/

/*void i2cSetSlaveTransmitHandler(u08 (*i2cSlaveTx_func)
  (u08 transmitDataLengthMax, u08* transmitData))
{
  i2cSlaveTransmit = i2cSlaveTx_func;
}*/

static inline void i2cSendStart(void)
{
  // Send start condition
  outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT) | BV(TWSTA));
}

static inline void i2cSendStop(void)
{
  // Transmit stop condition.
  // Leave with TWEA on for slave receiving.
  outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT) | BV(TWEA) | BV(TWSTO));
}

static inline void i2cWaitForComplete(void)
{
  // Wait for i2c interface to complete operation
  while (!(inb(TWCR) & BV(TWINT)));
}

static inline void i2cSendByte(u08 data)
{
  // Save data to the TWDR
  outb(TWDR, data);
  // Begin send
  outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT));
}

static inline void i2cReceiveByte(u08 ackFlag)
{
  // Begin receive over i2c
  if (ackFlag)
  {
    // ackFlag = TRUE: ACK the received data
    outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT) | BV(TWEA));
  }
  else
  {
    // ackFlag = FALSE: NACK the received data
    outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT));
  }
}

static inline u08 i2cGetReceivedByte(void)
{
  // Retrieve received data byte from i2c TWDR
  return (inb(TWDR));
}

/*static inline u08 i2cGetStatus(void)
{
  // Retrieve current i2c status from i2c TWSR
  return (inb(TWSR));
}*/

/*void i2cMasterSend(u08 deviceAddr, u08 length, u08* data)
{
  u08 i;

  // Wait for interface to be ready
  while (I2cState);
  // Set state
  I2cState = I2C_MASTER_TX;
  // Save data
  I2cDeviceAddrRW = (deviceAddr & 0xfe);  // RW cleared: write operation
  for (i = 0; i < length; i++)
    I2cSendData[i] = *data++;
  I2cSendDataIndex = 0;
  I2cSendDataLength = length;
  // Send start condition
  i2cSendStart();
}*/

/*void i2cMasterReceive(u08 deviceAddr, u08 length, u08* data)
{
  u08 i;

  // Wait for interface to be ready
  while (I2cState);
  // Set state
  I2cState = I2C_MASTER_RX;
  // Save data
  I2cDeviceAddrRW = (deviceAddr | 0x01);  // RW set: read operation
  I2cReceiveDataIndex = 0;
  I2cReceiveDataLength = length;
  // Send start condition
  i2cSendStart();
  // Wait for data
  while (I2cState);
  // Return data
  for (i = 0; i < length; i++)
    *data++ = I2cReceiveData[i];
}*/

u08 i2cMasterSendNI(u08 deviceAddr, u08 length, u08* data)
{
  u08 retval = I2C_OK;

  // Disable TWI interrupt
  cbi(TWCR, TWIE);

  // Send start condition
  i2cSendStart();
  i2cWaitForComplete();

  // Send device address with write
  i2cSendByte(deviceAddr & 0xfe);
  i2cWaitForComplete();

  // Check if device is present and live
  if (inb(TWSR) == TW_MT_SLA_ACK)
  {
    // Send data
    while (length)
    {
      i2cSendByte(*data++);
      i2cWaitForComplete();
      length--;
    }
  }
  else
  {
    // Device did not ACK its address. Data will not be transferred.
    retval = I2C_ERROR_NODEV;
  }

  // Transmit stop condition. Leave with TWEA on for slave receiving.
  i2cSendStop();
  while (!(inb(TWCR) & BV(TWSTO)));

  // Enable TWI interrupt
  sbi(TWCR, TWIE);

  return retval;
}

u08 i2cMasterReceiveNI(u08 deviceAddr, u08 length, u08 *data)
{
  u08 retval = I2C_OK;

  // Disable TWI interrupt
  cbi(TWCR, TWIE);

  // Send start condition
  i2cSendStart();
  i2cWaitForComplete();

  // Send device address with read
  i2cSendByte(deviceAddr | 0x01);
  i2cWaitForComplete();

  // Check if device is present and live
  if (inb(TWSR) == TW_MR_SLA_ACK)
  {
    // Accept receive data and ack it
    while (length > 1)
    {
      i2cReceiveByte(TRUE);
      i2cWaitForComplete();
      *data++ = i2cGetReceivedByte();
      // Decrement length
      length--;
    }

    // Accept receive data and nack it (last-byte signal)
    i2cReceiveByte(FALSE);
    i2cWaitForComplete();
    *data++ = i2cGetReceivedByte();
  }
  else
  {
    // Device did not ACK its address. Data will not be transferred.
    retval = I2C_ERROR_NODEV;
  }

  // Transmit stop condition. Leave with TWEA on for slave receiving.
  i2cSendStop();

  // Enable TWI interrupt
  sbi(TWCR, TWIE);

  return retval;
}

/* void i2cMasterTransferNI(u08 deviceAddr, u08 sendlength, u08* senddata,
  u08 receivelength, u08* receivedata)
{
  // Disable TWI interrupt
  cbi(TWCR, TWIE);

  // Send start condition
  i2cSendStart();
  i2cWaitForComplete();

  // If there's data to be sent, do it
  if (sendlength)
  {
    // Send device address with write
    i2cSendByte(deviceAddr & 0xfe);
    i2cWaitForComplete();

    // Send data
    while (sendlength)
    {
      i2cSendByte(*senddata++);
      i2cWaitForComplete();
      sendlength--;
    }
  }

  // If there's data to be received, do it
  if (receivelength)
  {
    // Send repeated start condition
    i2cSendStart();
    i2cWaitForComplete();

    // Send device address with read
    i2cSendByte(deviceAddr | 0x01);
    i2cWaitForComplete();

    // Accept receive data and ack it
    while (receivelength > 1)
    {
      i2cReceiveByte(TRUE);
      i2cWaitForComplete();
      *receivedata++ = i2cGetReceivedByte();
      // Decrement length
      receivelength--;
    }

    // Accept receive data and nack it (last-byte signal)
    i2cReceiveByte(TRUE);
    i2cWaitForComplete();
    *receivedata++ = i2cGetReceivedByte();
  }

  // Transmit stop condition.
  // Leave with TWEA on for slave receiving.
  i2cSendStop();
  while (!(inb(TWCR) & BV(TWSTO)));

  // Enable TWI interrupt
  sbi(TWCR, TWIE);
}
*/

//! I2C (TWI) interrupt service routine
SIGNAL(TWI_vect)
{
  // Read status bits
  u08 status = inb(TWSR) & TWSR_STATUS_MASK;

  switch (status)
  {
  // Master General
  case TW_START:			// 0x08: Sent start condition
  case TW_REP_START:			// 0x10: Sent repeated start condition
    DEBUGIP("I2C: M->START");
    // Send device address
    i2cSendByte(I2cDeviceAddrRW);
    break;

  // Master Transmitter & Receiver status codes
  case TW_MT_SLA_ACK:			// 0x18: Slave address acknowledged
  case TW_MT_DATA_ACK:			// 0x28: Data acknowledged
    DEBUGIP("I2C: MT->SLA_ACK or DATA_ACK");
    if (I2cSendDataIndex < I2cSendDataLength)
    {
      // Send data
      i2cSendByte(I2cSendData[I2cSendDataIndex++]);
    }
    else
    {
      // Transmit stop condition, enable SLA ACK
      i2cSendStop();
      // Set state
      I2cState = I2C_IDLE;
    }
    break;

  case TW_MR_DATA_NACK:			// 0x58: Data received, NACK reply issued
    DEBUGIP("I2C: MR->DATA_NACK");
    // Store final received data byte
    I2cReceiveData[I2cReceiveDataIndex++] = inb(TWDR);
    // Continue to transmit STOP condition
  case TW_MR_SLA_NACK:			// 0x48: Slave address not acknowledged
  case TW_MT_SLA_NACK:			// 0x20: Slave address not acknowledged
  case TW_MT_DATA_NACK:			// 0x30: Data not acknowledged
    DEBUGIP("I2C: MTR->SLA_NACK or MT->DATA_NACK");
    // Transmit stop condition, enable SLA ACK
    i2cSendStop();
    // Set state
    I2cState = I2C_IDLE;
    break;

  case TW_MT_ARB_LOST:			// 0x38: Bus arbitration lost
  //case TW_MR_ARB_LOST:		// 0x38: Bus arbitration lost
    DEBUGIP("I2C: MT->ARB_LOST");
    // Release bus
    outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT));
    // Set state
    I2cState = I2C_IDLE;
    // Release bus and transmit start when bus is free
    //outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT) | BV(TWSTA));
    break;

  case TW_MR_DATA_ACK:			// 0x50: Data acknowledged
    DEBUGIP("I2C: MR->DATA_ACK");
    // Store received data byte
    ; uint8_t x = inb(TWDR);
    I2cReceiveData[I2cReceiveDataIndex++] = x;
    uart_putw_hex(x);
    // Fall-through to see if more bytes will be received
  case TW_MR_SLA_ACK:			// 0x40: Slave address acknowledged
    DEBUGIP("I2C: MR->SLA_ACK");
    if (I2cReceiveDataIndex < I2cReceiveDataLength - 1)
      // Data byte will be received, reply with ACK (more bytes in transfer)
      i2cReceiveByte(TRUE);
    else
      // Data byte will be received, reply with NACK (final byte in transfer)
      i2cReceiveByte(FALSE);
    break;

  // Slave Receiver status codes
  case TW_SR_SLA_ACK:			// 0x60: own SLA+W received, ACK returned
  case TW_SR_ARB_LOST_SLA_ACK:		// 0x68: own SLA+W received, ACK returned
  case TW_SR_GCALL_ACK:			// 0x70:     GCA+W received, ACK returned
  case TW_SR_ARB_LOST_GCALL_ACK:	// 0x78:     GCA+W received, ACK returned
    DEBUGIP("I2C: SR->SLA_ACK");
    // We are addressed as slave for writing (data will be received from master)
    // Set state
    I2cState = I2C_SLAVE_RX;
    // Prepare buffer
    I2cReceiveDataIndex = 0;
    // Receive data byte and return ACK
    outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT) | BV(TWEA));
    break;

  case TW_SR_DATA_ACK:			// 0x80: data byte received, ACK returned
  case TW_SR_GCALL_DATA_ACK:		// 0x90: data byte received, ACK returned
    DEBUGIP("I2C: SR->DATA_ACK");
    // Get previously received data byte
    I2cReceiveData[I2cReceiveDataIndex++] = inb(TWDR);
    // Check receive buffer status
    if (I2cReceiveDataIndex < I2C_RECEIVE_DATA_BUFFER_SIZE)
    {
      // Receive data byte and return ACK
      i2cReceiveByte(TRUE);
      //outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT) | BV(TWEA));
    }
    else
    {
      // Receive data byte and return NACK
      i2cReceiveByte(FALSE);
      //outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT));
    }
    break;

  case TW_SR_DATA_NACK:			// 0x88: data byte received, NACK returned
  case TW_SR_GCALL_DATA_NACK:		// 0x98: data byte received, NACK returned
    DEBUGIP("I2C: SR->DATA_NACK");
    // Receive data byte and return NACK
    i2cReceiveByte(FALSE);
    //outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT));
    break;

  case TW_SR_STOP:			// 0xa0: STOP or REPEATED START received
					//       while addressed as slave
    DEBUGIP("I2C: SR->SR_STOP");
    // Switch to SR mode with SLA ACK
    outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT) | BV(TWEA));
    // i2c receive is complete, call i2cSlaveReceive
    if (i2cSlaveReceive)
      i2cSlaveReceive(I2cReceiveDataIndex, I2cReceiveData);
    // Set state
    I2cState = I2C_IDLE;
    break;

  // Slave Transmitter
  case TW_ST_SLA_ACK:			// 0xa8: own SLA+R received, ACK returned
  case TW_ST_ARB_LOST_SLA_ACK:		// 0xb0:     GCA+R received, ACK returned
    DEBUGIP("I2C: ST->SLA_ACK");
    // We are addressed as slave for reading (data must be xmit back to master)
    // Set state
    I2cState = I2C_SLAVE_TX;
    // Request data from application
    if (i2cSlaveTransmit)
      I2cSendDataLength = i2cSlaveTransmit(I2C_SEND_DATA_BUFFER_SIZE, I2cSendData);
    // Reset data index
    I2cSendDataIndex = 0;
    // Fall-through to transmit first data byte
  case TW_ST_DATA_ACK:			// 0xb8: data byte transmitted, ACK received
    DEBUGIP("I2C: ST->DATA_ACK");
    // Transmit data byte
    outb(TWDR, I2cSendData[I2cSendDataIndex++]);
    if (I2cSendDataIndex < I2cSendDataLength)
      // Expect ACK to data byte
      outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT) | BV(TWEA));
    else
      // Expect NACK to data byte
      outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT));
    break;

  case TW_ST_DATA_NACK:			// 0xc0: data byte transmitted, NACK received
  case TW_ST_LAST_DATA:			// 0xc8:
    DEBUGIP("I2C: ST->DATA_NACK or LAST_DATA");
    // All done
    // Switch to open slave
    outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT) | BV(TWEA));
    // Set state
    I2cState = I2C_IDLE;
    break;

  // Misc status
  case TW_NO_INFO:			// 0xf8: No relevant state information
    // Do nothing
    DEBUGIP("I2C: NO_INFO");
    break;
  case TW_BUS_ERROR:			// 0x00: Bus error (illegal start/stop condition?)
    DEBUGIP("I2C: BUS_ERROR");
    // Reset internal hardware and release bus
    outb(TWCR, (inb(TWCR) & TWCR_CMD_MASK) | BV(TWINT) | BV(TWSTO) | BV(TWEA));
    // Set state
    I2cState = I2C_IDLE;
    break;
  }
}

/*eI2cStateType i2cGetState(void)
{
  return I2cState;
}*/
