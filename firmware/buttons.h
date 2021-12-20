//*****************************************************************************
// Filename : 'buttons.h'
// Title    : Definitions for MONOCHRON buttons handling
//*****************************************************************************

#ifndef BUTTONS_H
#define BUTTONS_H

// The Monochron buttons
#define BTN_NONE	0x00
#define BTN_MENU	0x01
#define BTN_SET		0x02
#define BTN_PLUS	0x04

// Start async button ADC conversion
void btnConvStart(void);

// Button initialization
void btnInit(void);
#endif
