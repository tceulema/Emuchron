//*****************************************************************************
// Filename : 'ks0108conf.h'
// Title    : Graphic lcd driver for HD61202/KS0108 displays
//*****************************************************************************

#ifndef KS0108CONF_H
#define KS0108CONF_H

// Define lcd hardware interface
// - LCD_MEMORY_INTERFACE assumes that the registers of the lcd have been mapped
//   into the external memory space of the AVR processor memory bus
// - LCD_PORT_INTERFACE is a direct-connection interface from port pins to lcd
//   SELECT (UNCOMMENT) ONLY ONE!
// *** NOTE: memory interface is not yet fully supported, but it might work
//#define GLCD_MEMORY_INTERFACE
#define GLCD_PORT_INTERFACE

// GLCD_PORT_INTERFACE specifics
#ifdef GLCD_PORT_INTERFACE

// Make sure these parameters are not already defined elsewhere
#ifndef GLCD_CTRL_PORT
#define GLCD_CTRL_RS			7
#define GLCD_CTRL_RS_PORT		PORTB
#define GLCD_CTRL_RS_DDR		DDRB

#define GLCD_CTRL_RW			5
#define GLCD_CTRL_RW_PORT		PORTB
#define GLCD_CTRL_RW_DDR		DDRB

#define GLCD_CTRL_E			4
#define GLCD_CTRL_E_PORT		PORTB
#define GLCD_CTRL_E_DDR			DDRB

#define GLCD_CTRL_CS0			0
#define GLCD_CTRL_CS0_PORT		PORTC
#define GLCD_CTRL_CS0_DDR		DDRC

#define GLCD_CTRL_CS1			2
#define GLCD_CTRL_CS1_PORT		PORTD
#define GLCD_CTRL_CS1_DDR		DDRD

// This too
//#define GLCD_CTRL_RESET 		PC4
//#define GLCD_CTRL_RESET_PORT		PORTC
//#define GLCD_CTRL_RESET_DDR		DDRC
// (*) NOTE: additional controller chip selects are optional and 
// will be automatically used per each step in 64 pixels of display size
// Example: Display with 128 horizontal pixels uses 2 controllers
#endif
#ifndef GLCD_DATA_PORT
// We split the data port into two 4-byte chunks so we dont bash the SPI or RXTX pins
//#define GLCD_DATA_PORT		PORTD	// PORT for lcd data signals
//#define GLCD_DATA_DDR			DDRD	// DDR register of LCD_DATA_PORT
//#define GLCD_DATA_PIN			PIND	// PIN register of LCD_DATA_PORT

#define GLCD_DATAH_PORT			PORTD	// PORT for lcd data signals
#define GLCD_DATAH_DDR			DDRD	// DDR register of LCD_DATA_PORT
#define GLCD_DATAH_PIN			PIND	// PIN register of LCD_DATA_PORT
#define GLCD_DATAL_PORT			PORTB	// PORT for lcd data signals
#define GLCD_DATAL_DDR			DDRB	// DDR register of LCD_DATA_PORT
#define GLCD_DATAL_PIN			PINB	// PIN register of LCD_DATA_PORT
#endif
#endif

// GLCD_MEMORY_INTERFACE specifics
#ifdef GLCD_MEMORY_INTERFACE
// Make sure these parameters are not already defined elsewhere
#ifndef GLCD_CONTROLLER0_CTRL_ADDR
// Absolute address of lcd Controller #0 CTRL and DATA registers
#define GLCD_CONTROLLER0_CTRL_ADDR	0x1000
#define GLCD_CONTROLLER0_DATA_ADDR	0x1001
// Offset of other controllers with respect to controller 0
#define GLCD_CONTROLLER_ADDR_OFFSET	0x0002
#endif
#endif

// Lcd geometry defines (change these definitions to adapt code/settings)
#define GLCD_XPIXELS			128	// Pixel width of entire display
#define GLCD_YPIXELS			64	// Pixel height of entire display

// Lcd defines for a single display controller
#define GLCD_CONTROLLER_XPIXELS		64	// Pixel width
#define GLCD_CONTROLLER_YPIXELS		64	// Pixel height
#define GLCD_CONTROLLER_YPAGES		8	// Pixel height pages
#define GLCD_CONTROLLER_XPIXBITS	6	// Bits used for x pixels
#define GLCD_CONTROLLER_XPIXMASK	0x3F	// Mask used for x pixels
#define GLCD_CONTROLLER_YPIXMASK	0x3F	// Mask used for y pixels
#define GLCD_CONTROLLER_YPAGEBITS	3	// Bits used for y pixel page
#define GLCD_CONTROLLER_YPAGEMASK	0x07	// Mask used for y pixel page

// Set text size of display
// These definitions are not currently used and will probably move to glcd.h
#define GLCD_TEXT_LINES			8	// Visible lines
#define GLCD_TEXT_LINE_LENGTH		22	// Internal line length
#endif
