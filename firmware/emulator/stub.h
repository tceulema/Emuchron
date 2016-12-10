//*****************************************************************************
// Filename : 'stub.h'
// Title    : Stub definitions for emuchron emulator
//*****************************************************************************

#ifndef STUB_H
#define STUB_H

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "../avrlibtypes.h"

// Overide (ignore) AVR progmem directive
#define progmem

// Misc stubs for hardware related stuff
#define RXEN0			0
#define TXEN0			1
#define USBS0			0
#define UCSZ00			1
#define BRRL_192		0
#define CS00			0
#define CS01			1
#define OCIE0A			0
#define WGM01			1
#define WGM20			0
#define WGM21			1
#define WGM22			2
#define COM2B1			3
#define WDTO_2S			2000
#define CS20			0
#define CS21			1
#define CS22			2
#define TOIE2			0

// Overide debug reporting methods
#define putstring_nl(x)		stubPutString((x),"%s\n")
#define putstring(x)		stubPutString((x),"%s")
#define uart_put_dec(x)		stubUartPutDec((int)(x),"%d")
#define uart_putdw_dec(x)	stubUartPutDec((int)(x),"%d")
#define uart_putw_dec(x)	stubUartPutDec((int)(x),"%d")
#define uart_putc_hex(x)	stubUartPutDec((int)(x),"%x")
#define uart_putchar(x)		stubUartPutChar((x))

// Overide eeprom methods
#define eeprom_read_byte(x)	stubEepRead((x))
#define eeprom_write_byte(x,y)	stubEepWrite((x),(y))

// Overide RTC methods
#define i2cMasterReceiveNI(x, y, z)	stubI2cMasterReceiveNI((x), (y), (z))
#define i2cMasterSendNI(x, y, z)	stubI2cMasterSendNI((x), (y), (z))

// Delay stub
#define _delay_ms(x)		stubDelay((x))

// Emulator timer stub cycle state
#define CYCLE_NOWAIT		0
#define CYCLE_WAIT		1
#define CYCLE_WAIT_STATS	2
#define CYCLE_REQ_NOWAIT	3
#define CYCLE_REQ_WAIT		4

// Keyboard input mode
#define KB_MODE_LINE		0
#define KB_MODE_SCAN		1

// Misc stub stuff
#define _BV(x) 			(0x01 << (x))
#define pgm_read_byte(x) 	((u08)(*(x)))
#define asm			__asm__ 

// Eeprom stubs
uint8_t stubEepRead(uint8_t *eprombyte);
void stubEepWrite(uint8_t *eprombyte, uint8_t value);
void stubEepReset(void);

// Beep stub
void stubBeep(uint16_t hz, uint16_t msec);

// Delay stub
void stubDelay(int x);

// RTC interface stubs
int stubTimeSet(uint8_t sec, uint8_t min, uint8_t hr, uint8_t day,
  uint8_t date, uint8_t mon, uint8_t yr);
u08 stubI2cMasterReceiveNI(u08 deviceAddr, u08 length, u08 *data);
u08 stubI2cMasterSendNI(u08 deviceAddr, u08 length, u08* data);

// Misc debug stubs
void stubPutString(char *x, char *format);
void stubUartPutChar(char x);
void stubUartPutDec(int x, char *format);

// Entry points for stubbed functionality in monomain.c [firmware]
int monoMain(void);
void monoTimer(void);

// Misc functions that are stubbed empty
void buttonsInit();
void i2cInit();
void uart_init(uint16_t x);
void wdt_disable();
void wdt_enable(uint16_t x);
void wdt_reset();

// Below is emulator oriented stub functionality
// Monochron emulator stubs
char stubEventGet(void);
void stubEventInit(int startMode, void (*stubHelpHandler)(void));

// Alarm sound and switch stubs
void alarmSoundReset(void);
void alarmSoundStop(void);
void alarmSwitchSet(uint8_t on, uint8_t show);
void alarmSwitchShow(void);
void alarmSwitchToggle(uint8_t show);

// Keyboard input and keypress events
char kbKeypressScan(u08 quitFind);
int kbModeGet(void);
void kbModeSet(int);

// Delay and timer functions
char waitDelay(int delay);
char waitKeypress(int allowQuit);
char waitTimerExpiry(struct timeval *tvTimer, int expiry, int allowQuit,
  suseconds_t *remaining);
void waitTimerStart(struct timeval *tvTimer);

// Statistics
void stubStatsPrint(void);
void stubStatsReset(void);

// Help pages when running the clock or Monochron emulator
void stubHelpClockFeed(void);
void stubHelpMonochron(void);
#endif
