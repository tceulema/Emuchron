//*****************************************************************************
// Filename : 'dali.h'
// Title    : Defs for MONOCHRON xdali clock
//*****************************************************************************

#ifndef DALI_H
#define DALI_H

#include "../avrlibtypes.h"

// Dali digit transition draw steps
#define DALI_GEN_CYCLES		32

// Dali clock
void daliButton(u08 pressedButton);
void daliCycle(void);
void daliInit(u08 mode);
#endif
