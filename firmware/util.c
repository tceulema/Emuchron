//*****************************************************************************
// Filename : 'util.c'
// Title    : UART I/O utility functions for MONOCHRON
//*****************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "monomain.h"
#include "util.h"

// Creates a 8N1 UART connect
// Remember that the BBR is #defined for each F_CPU in util.h
void uart_init(uint16_t BRR)
{
  UBRR0 = BRR;               // set baudrate counter

  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
  UCSR0C = _BV(USBS0) | (3<<UCSZ00);
  DDRD |= _BV(1);
  DDRD &= ~_BV(0);
}

int uart_putchar(char c)
{
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
}

void uart_putc_hex(uint8_t b)
{
  /* upper nibble */
  if((b >> 4) < 0x0a)
    uart_putc((b >> 4) + '0');
  else
    uart_putc((b >> 4) - 0x0a + 'a');

  /* lower nibble */
  if((b & 0x0f) < 0x0a)
    uart_putc((b & 0x0f) + '0');
  else
    uart_putc((b & 0x0f) - 0x0a + 'a');
}

void uart_putw_hex(uint16_t w)
{
  uart_putc_hex((uint8_t)(w >> 8));
  uart_putc_hex((uint8_t)(w & 0xff));
}

char uart_getchar(void)
{
  while (!(UCSR0A & _BV(RXC0)));
  return UDR0;
}

char uart_getch(void)
{
  return (UCSR0A & _BV(RXC0));
}

// The following functions are used only in debugging mode. Not building
// the function block of these functions when it is not needed anyway will
// save us quite a few bytes.
// The compiler is smart enough to omit the generation of the function block
// code based on the master debugging flag. I do admit this looks ugly. 

void ROM_putstring(const char *str, uint8_t nl)
{
  if (DEBUGGING == 1)
  {
    uint8_t i;

    for (i = 0; pgm_read_byte(&str[i]); i++)
    {
      uart_putchar(pgm_read_byte(&str[i]));
    }
    if (nl)
    {
      uart_putchar('\n');
      uart_putchar('\r');
    }
  }
}

void uart_putdw_hex(uint32_t dw)
{
  if (DEBUGGING == 1)
  {
    uart_putw_hex((uint16_t)(dw >> 16));
    uart_putw_hex((uint16_t)(dw & 0xffff));
  }
}

void uart_putw_dec(uint16_t w)
{
  if (DEBUGGING == 1)
  {
    uint16_t num = 10000;
    uint8_t started = 0;

    while (num > 0)
    {
      uint8_t b = w / num;
      if (b > 0 || started || num == 1)
      {
        uart_putc('0' + b);
        started = 1;
      }
      w -= b * num;
      num /= 10;
    }
  }
}

void uart_put_dec(int8_t w)
{
  if (DEBUGGING == 1)
  {
    uint16_t num = 100;
    uint8_t started = 0;

    if (w < 0)
    {
      uart_putc('-');
      w *= -1;
    }
    while (num > 0)
    {
      int8_t b = w / num;
      if (b > 0 || started || num == 1)
      {
        uart_putc('0' + b);
        started = 1;
      }
      w -= b * num;
      num /= 10;
    }
  }
}

void uart_putdw_dec(uint32_t dw)
{
  if (DEBUGGING == 1)
  {
    uint32_t num = 1000000000;
    uint8_t started = 0;

    while (num > 0)
    {
      uint8_t b = dw / num;
      if (b > 0 || started || num == 1)
      {
        uart_putc('0' + b);
        started = 1;
      }
      dw -= b * num;
      num /= 10;
    }
  }
}

