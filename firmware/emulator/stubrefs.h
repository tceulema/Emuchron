//*****************************************************************************
// Filename : 'stubrefs.h'
// Title    : Stub references for emuchron emulator
//*****************************************************************************

#ifndef STUBREFS_H
#define STUBREFS_H

#include "../avrlibtypes.h"

// Hardware stubs
extern uint8_t MCUSR;
extern uint8_t DDRB;
extern uint8_t DDRC;
extern uint8_t DDRD;
extern uint8_t UDR0;
extern uint16_t UBRR0;
extern uint8_t UCSR0A;
extern uint8_t UCSR0B;
extern uint8_t UCSR0C;
extern uint8_t TCCR0A;
extern uint8_t TCCR0B;
extern uint8_t OCR0A;
extern uint8_t OCR2A;
extern uint8_t OCR2B;
extern uint8_t TIMSK0;
extern uint8_t TIMSK2;
extern uint8_t TCCR1B;
extern uint8_t TCCR2A;
extern uint8_t TCCR2B;
extern uint8_t PORTB;
extern uint8_t PORTC;
extern uint8_t PORTD;
extern uint8_t PINB;
extern uint8_t PIND;

// Ignore several AVR specific directives and functions
#define progmem
#define PROGMEM
#define asm(x)
#define cli()
#define sei()
#define loop_until_bit_is_set(x,y)
#define wdt_disable()
#define wdt_enable(x)
#define wdt_reset()

// Misc stubs for hardware related stuff
#define RXEN0			4
#define TXEN0			3
#define RXC0			7
#define USBS0			3
#define UCSZ00			1
#define CS00			0
#define CS01			1
#define CS20			0
#define CS21			1
#define CS22			2
#define OCIE0A			1
#define WGM01			1
#define WGM20			0
#define WGM21			1
#define WGM22			3
#define COM2B1			5
#define TOIE2			0
#define BRRL_192		0
#define WDTO_2S			2000

// Misc stubs related to memory operations
#define _BV(x) 			(0x01 << (x))
#define __LPM(x)		(*(x))
#define memcpy_P		memcpy
#define pgm_read_byte(x) 	((uint8_t)(*(x)))
#define pgm_read_word(x) 	((uint16_t)(*(x)))
#define pgm_read_dword(x) 	((uint32_t)(*(x)))
#define PSTR(x)			(x)
#endif
