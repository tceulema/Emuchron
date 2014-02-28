//*****************************************************************************
// Filename : 'mario.h'
// Title    : Defines for playing the world famous Mario melody
//*****************************************************************************

#ifndef MARIO_H
#define MARIO_H

// The tune tempo
#define MAR_TEMPO	95

// The factors used to fit tone and beat values in an unsigned char data
// type. The densed values are expanded using these very same factors.
// DO NOT CHANGE THESE as they require a rebuild of respectively MarioTones[]
// and MarioBeats[] contents.
#define MAR_TONEFACTOR	9
#define MAR_BEATFACTOR	3
#endif

