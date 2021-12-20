//*****************************************************************************
// Filename : 'i2c.h'
// Title    : I2C interface using AVR Two-Wire Interface (TWI) hardware
//*****************************************************************************

#ifndef I2C_H
#define I2C_H

#include "avrlibtypes.h"

// Return values
#define I2C_OK			0x00
#define I2C_ERROR_NODEV		0x01

// Types
typedef enum
{
  I2C_IDLE = 0,
  I2C_BUSY = 1,
  I2C_MASTER_TX = 2,
  I2C_MASTER_RX = 3,
  I2C_SLAVE_TX = 4,
  I2C_SLAVE_RX = 5
} eI2cStateType;

// Initialize I2C (TWI) interface
void i2cInit(void);

/*// Set the I2C transaction bitrate (in KHz)
void i2cSetBitrate(u16 bitrateKHz);*/

// I2C setup and configurations commands
// Set the local (AVR processor's) I2C device address
//void i2cSetLocalDeviceAddr(u08 deviceAddr, u08 genCallEn);

// Set the user function which handles receiving (incoming) data as a slave
//void i2cSetSlaveReceiveHandler
//  (void (*i2cSlaveRx_func)(u08 receiveDataLength, u08* receiveData));
// Set the user function which handles transmitting (outgoing) data as a slave
//void i2cSetSlaveTransmitHandler
//  (u08 (*i2cSlaveTx_func)(u08 transmitDataLengthMax, u08* transmitData));

// High-level I2C transaction commands
// Send I2C data to a device on the bus
//void i2cMasterSend(u08 deviceAddr, u08 length, u08 *data);
// Receive I2C data from a device on the bus
//void i2cMasterReceive(u08 deviceAddr, u08 length, u08* data);
// Send I2C data to a device on the bus (non-interrupt based)
u08 i2cMasterSendNI(u08 deviceAddr, u08 length, u08* data);
// Receive I2C data from a device on the bus (non-interrupt based)
u08 i2cMasterReceiveNI(u08 deviceAddr, u08 length, u08 *data);

// Get the current high-level state of the I2C interface
//eI2cStateType i2cGetState(void);
#endif
