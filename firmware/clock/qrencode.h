//*****************************************************************************
// Filename : 'qrencode.h'
// Title    : Defs for generating and accessing a QR
//*****************************************************************************

// The code supported by this header is designed for a redundancy 1 (L), level
// 2 (25x25) QR clock, allowing to encode a string up to 32 characters.
// For more info refer to qrencode.c [firmware/clock].

#ifndef QRENCODE_H
#define QRENCODE_H

// Define fixed QR environment: redundancy 1 (L), level 2 (25x25)
#define WD		25
#define WDB		4

// Access the qrframe buffer
#define QRBIT(x,y)	((qrframe[((x) >> 3) + (y) * WDB] >> (7 - ((x) & 7))) & 1)
#define SETQRBIT(x,y)	qrframe[((x) >> 3) + (y) * WDB] |= 0x80 >> ((x) & 7)
#define TOGQRBIT(x,y)	qrframe[((x) >> 3) + (y) * WDB] ^= 0x80 >> ((x) & 7)

// QR encode
void qrGenInit(void);
void qrMaskTry(unsigned char mask);
unsigned char qrMaskApply(void);
#endif
