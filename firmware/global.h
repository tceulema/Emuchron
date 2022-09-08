//*****************************************************************************
// Filename : 'global.h'
// Title    : AVRlib project global include and defines
//*****************************************************************************

#ifndef GLOBAL_H
#define GLOBAL_H

// Global avrlib/linux defines for Monochron and Emuchron
#include "avrlibdefs.h"
#include "avrlibtypes.h"
#include "util.h"
#ifdef EMULIN
#include "emulator/stub.h"
#include "emulator/stubrefs.h"
#endif

// Project/system dependent defines

// Version of our Emuchron project code base
#define EMUCHRON_VERSION		"v7.1"

// Our own definition of the Monochron/Emuchron truth.
// Why do we do this? It turns out that gcc sometimes get utterly confused
// using the TRUE and FALSE defines/macros resulting in bad object code.
// This may be due to the mixing of Monochron avr with Emuchron stdlib, ncurses
// and glut code.
// Even worse, similar issues also pop up in actual monochron firmware built
// in this environment by showing unexplained erratic behavior.
// In any case, I do not trust TRUE and FALSE in combination with this mixed
// avr, stdlib, ncurses and glut software environment.
#define MC_FALSE			0
#define MC_TRUE				1

// Debugging macros.
// Select to generate generic application debug output.
// 0 = Off, 1 = On
#define DEBUGGING			0
#define DEBUG(x)			if (DEBUGGING) { x; }
#define DEBUGP(x)			DEBUG(putstring_nl(x))
// Select to generate time change event debug output. Requires debug switch
// DEBUGGING to be set.
// 0 = Off, 1 = On
#define DEBUGTIME			1
#define DEBUGT(x)			if (DEBUGGING && DEBUGTIME) { x; }
#define DEBUGTP(x)			DEBUGT(putstring_nl(x))
// Select to generate i2c debug output.
// 0 = Off, 1 = On
#define DEBUGI2C			0
#define DEBUGI(x)			if (DEBUGI2C) { x; }
#define DEBUGIP(x)			DEBUGI(putstring_nl(x))

// Math macros
#define ABS(x)				(((x) > 0) ? (x) : -(x))
#define MAX(x,y)			(((x) > (y)) ? (x) : (y))
#define MIN(x,y)			(((x) < (y)) ? (x) : (y))
#define SIGN(x)				(((x) > 0) ? 1 : (((x) < 0) ? -1 : 0))

// Definitions for Atmel cpu with attached Monochron hardware components

// Display backlight. Allows software control of backlight, assuming you
// mounted your 100ohm resistor in R2.
// 0 = Off, 1 = On
#define BACKLIGHT_ADJUST		1

// Pin/port definitions for alarm switch
#define ALARM_PORT			PORTB
#define ALARM_PIN			PINB
#define ALARM_DDR			DDRB
#define ALARM				6

// Pin/port definitions for piezo speaker
#define PIEZO_PORT			PORTC
#define PIEZO_PIN			PINC
#define PIEZO_DDR			DDRC
#define PIEZO				3

// Pin/port definitions for TWI (i2c)
#define I2C_PORT			PORTC
#define I2C_SDA				4
#define I2C_SCL				5

// Pin/port definitions for direct-connection port interface to lcd controller
#define GLCD_CTRL_E_PORT		PORTB
#define GLCD_CTRL_E_DDR			DDRB
#define GLCD_CTRL_E			4

#define GLCD_CTRL_RW_PORT		PORTB
#define GLCD_CTRL_RW_DDR		DDRB
#define GLCD_CTRL_RW			5

#define GLCD_CTRL_RS_PORT		PORTB
#define GLCD_CTRL_RS_DDR		DDRB
#define GLCD_CTRL_RS			7

#define GLCD_CTRL_CS0_PORT		PORTC
#define GLCD_CTRL_CS0_DDR		DDRC
#define GLCD_CTRL_CS0			0

#define GLCD_CTRL_CS1_PORT		PORTD
#define GLCD_CTRL_CS1_DDR		DDRD
#define GLCD_CTRL_CS1			2

#define GLCD_DATAH_PORT			PORTD	// PORT for high lcd data signals
#define GLCD_DATAH_DDR			DDRD	// DDR register of LCD_DATA_PORT
#define GLCD_DATAH_PIN			PIND	// PIN register of LCD_DATA_PORT
#define GLCD_DATAL_PORT			PORTB	// PORT for low lcd data signals
#define GLCD_DATAL_DDR			DDRB	// DDR register of LCD_DATA_PORT
#define GLCD_DATAL_PIN			PINB	// PIN register of LCD_DATA_PORT

// Lcd backlight brightness related constants
#define OCR2B_BITSHIFT			0
#define OCR2B_PLUS			1
#define OCR2A_VALUE			16
#endif
