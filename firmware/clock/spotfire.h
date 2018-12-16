//*****************************************************************************
// Filename : 'spotfire.h'
// Title    : Generic defs for MONOCHRON spotfire clocks
//*****************************************************************************

#ifndef SPOTFIRE_H
#define SPOTFIRE_H

// Common defines for bars in barchart and cascade plot
#define SPOT_BAR_Y_START	54
#define SPOT_BAR_HEIGHT_MAX	29
#define SPOT_BAR_VAL_STEPS	59
#define SPOT_BAR_VAL_Y_OFFSET	-8

// Spotfire clock common utility functions
void spotAxisInit(u08 clockId);
void spotBarUpdate(u08 x, u08 width, u08 oldVal, u08 newVal, s08 valXOffset,
  u08 fillType);
void spotCommonInit(char *label, u08 mode);
u08 spotCommonUpdate(void);
#endif
