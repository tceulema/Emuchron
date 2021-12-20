//*****************************************************************************
// Filename : 'avrlibdefs.h'
// Title    : AVRlib global defines and macros include file
//*****************************************************************************

#ifndef AVRLIBDEFS_H
#define AVRLIBDEFS_H

#define	inb(addr)		(addr)
#define	outb(addr, data)	addr = (data)
#define cbi(reg,bit)		reg &= ~(_BV(bit))
#define sbi(reg,bit)		reg |= (_BV(bit))
#define DDR(addr)		((addr)-1)    // Port data direction register address
#define PIN(addr)		((addr)-2)    // Port input register address
#endif
