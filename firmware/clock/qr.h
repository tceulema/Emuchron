//*****************************************************************************
// Filename : 'qr.h'
// Title    : Defs for MONOCHRON QR clock
//*****************************************************************************

#ifndef QR_H
#define QR_H

// The number of clock cycles needed to create and display a QR
#define QR_GEN_CYCLES	6

// QR clock
void qrCycle(void);
void qrInit(u08 mode);
#endif
