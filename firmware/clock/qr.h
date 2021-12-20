//*****************************************************************************
// Filename : 'qr.h'
// Title    : Defs for MONOCHRON QR clock
//*****************************************************************************

#ifndef QR_H
#define QR_H

#include "../avrlibtypes.h"

// The number of clock cycles needed to create and display a QR
#define QR_GEN_CYCLES	5

// QR clock
void qrCycle(void);
void qrInit(u08 mode);
#endif
