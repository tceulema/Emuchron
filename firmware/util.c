//*****************************************************************************
// Filename : 'util.c'
// Title    : UART I/O utility functions for MONOCHRON
//*****************************************************************************

#include "global.h"
#include "util.h"

// Local function prototype
static void uart_dec(uint32_t dw, uint32_t num);

//
// Function: uart_init
//
// Creates a 8N1 UART connect.
// Remember that the BRR is #defined for each F_CPU in util.h [firmware].
//
void uart_init(uint16_t BRR)
{
  // Set baudrate counter
  UBRR0 = BRR;

  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
  UCSR0C = _BV(USBS0) | (3 << UCSZ00);
  DDRD |= _BV(1);
  DDRD &= ~_BV(0);
}

//
// Function: uart_putchar
//
// Put a single character. This is the base function used by other functions to
// put numbers and strings.
//
void uart_putchar(char c)
{
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
#ifdef EMULIN
  stubUartPutChar();
#endif
}

//
// Function: uart_getchar
//
// Wait for a char and read it
//
char uart_getchar(void)
{
  while (!(UCSR0A & _BV(RXC0)));
  return UDR0;
}

//
// Function: uart_getch
//
// Scan for the presence of a char
//
char uart_getch(void)
{
  return (UCSR0A & _BV(RXC0));
}

// The following functions are used only in debugging mode. Not building the
// function block of these functions when it is not needed anyway will save us
// quite a few bytes.
// The compiler is smart enough to omit the generation of the function block
// code based on the master debugging flag. I do admit this looks ugly.

//
// Function: ROM_putstring
//
// Put a progmem string and add a nl if needed
//
void ROM_putstring(const char *str, uint8_t nl)
{
  if (DEBUGGING == 1 || DEBUGI2C == 1)
  {
    uint8_t i;

    for (i = 0; pgm_read_byte(&str[i]); i++)
      uart_putchar(pgm_read_byte(&str[i]));
    if (nl)
    {
      uart_putchar('\n');
      uart_putchar('\r');
    }
  }
}

//
// Function: uart_put_hex
//
// Put an 8-bit number in hex format
//
void uart_put_hex(uint8_t b)
{
  if (DEBUGGING == 1 || DEBUGI2C == 1)
  {
    /* Upper nibble */
    if ((b >> 4) < 0x0a)
      uart_putchar((b >> 4) + '0');
    else
      uart_putchar((b >> 4) - 0x0a + 'a');

    /* Lower nibble */
    if ((b & 0x0f) < 0x0a)
      uart_putchar((b & 0x0f) + '0');
    else
      uart_putchar((b & 0x0f) - 0x0a + 'a');
  }
}

//
// Function: uart_putw_hex
//
// Put a 16-bit number in hex format
//
void uart_putw_hex(uint16_t w)
{
  if (DEBUGGING == 1 || DEBUGI2C == 1)
  {
    uart_put_hex((uint8_t)(w >> 8));
    uart_put_hex((uint8_t)(w & 0xff));
  }
}

//
// Function: uart_putdw_hex
//
// Put a 32-bit number in hex format
//
void uart_putdw_hex(uint32_t dw)
{
  if (DEBUGGING == 1 || DEBUGI2C == 1)
  {
    uart_putw_hex((uint16_t)(dw >> 16));
    uart_putw_hex((uint16_t)(dw & 0xffff));
  }
}

//
// Function: uart_put_sdec
//
// Put a signed 8-bit number in decimal format
//
void uart_put_sdec(int8_t b)
{
  if (DEBUGGING == 1 || DEBUGI2C == 1)
  {
    if (b < 0)
    {
      uart_putchar('-');
      b = -b;
    }
    uart_dec((uint32_t)b, 100);
  }
}

//
// Function: uart_put_dec
//
// Put an unsigned 8-bit number in decimal format
//
void uart_put_dec(uint8_t b)
{
  if (DEBUGGING == 1 || DEBUGI2C == 1)
  {
    uart_dec((uint32_t)b, 100);
  }
}

//
// Function: uart_putw_dec
//
// Put an unsigned 16-bit number in decimal format
//
void uart_putw_dec(uint16_t w)
{
  if (DEBUGGING == 1 || DEBUGI2C == 1)
  {
    uart_dec((uint32_t)w, 10000);
  }
}

//
// Function: uart_putdw_dec
//
// Put an unsigned 32-bit number in decimal format
//
void uart_putdw_dec(uint32_t dw)
{
  if (DEBUGGING == 1 || DEBUGI2C == 1)
  {
    uart_dec(dw, 1000000000);
  }
}

//
// Function: uart_dec
//
// Put a number in decimal format using a divider representing an 8/16/32-bit
// size number
//
static void uart_dec(uint32_t dw, uint32_t num)
{
  if (DEBUGGING == 1 || DEBUGI2C == 1)
  {
    uint8_t started = 0;
    uint8_t digit;

    while (num > 0)
    {
      digit = dw / num;
      if (digit > 0 || started || num == 1)
      {
        uart_putchar('0' + digit);
        started = 1;
      }
      dw = dw - digit * num;
      num = num / 10;
    }
  }
}
