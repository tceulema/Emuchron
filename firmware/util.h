//*****************************************************************************
// Filename : 'util.h'
// Title    : Defines for utility routines for MONOCHRON
//*****************************************************************************

#ifndef UTIL_H
#define UTIL_H

#ifndef EMULIN
#include <avr/pgmspace.h>
#endif
#include "avrlibtypes.h"

// Raw constants for the UART to make the bit timing nice
#if (F_CPU == 16000000)
#define BRRL_9600 103    // for 16MHZ
#define BRRL_192 52    // for 16MHZ
#elif (F_CPU == 8000000)
#define BRRL_9600 52
#define BRRL_192 26
#endif

// By default we stick strings in ROM to save RAM
#define putstring(x) ROM_putstring(PSTR(x), 0)
#define putstring_nl(x) ROM_putstring(PSTR(x), 1)

// Init the UART
void uart_init(uint16_t BRR);

// Read char
char uart_getchar(void);

// Base function to put a single char
void uart_putchar(char c);

// Put number in hex format
void uart_put_hex(uint8_t b);
void uart_putw_hex(uint16_t w);
void uart_putdw_hex(uint32_t dw);

// Put number in decimal format
void uart_put_sdec(int8_t b);
void uart_put_dec(uint8_t b);
void uart_putw_dec(uint16_t w);
void uart_putdw_dec(uint32_t dw);

// Put ROM string with optional newline
void ROM_putstring(const char *str, uint8_t nl);
#endif
