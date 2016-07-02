//*****************************************************************************
// Filename : 'spotfire.h'
// Title    : Generic defs for MONOCHRON spotfire clocks
//*****************************************************************************

#ifndef SPOTFIRE_H
#define SPOTFIRE_H

// Spotfire clock common utility functions
void spotAxisInit(u08 clockId);
void spotBarUpdate(u08 x, u08 y, u08 maxVal, u08 maxHeight, u08 width,
  u08 oldVal, u08 newVal, s08 valXOffset, s08 valYOffset, u08 fillType);
void spotCommonInit(char *label, u08 mode);
u08 spotCommonUpdate(void);
#endif
