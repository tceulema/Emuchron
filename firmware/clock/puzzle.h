//*****************************************************************************
// Filename : 'puzzle.h'
// Title    : Defs for MONOCHRON puzzle clock
//*****************************************************************************

#ifndef PUZZLE_H
#define PUZZLE_H

#include "../avrlibtypes.h"

// Puzzle clock
void puzzleButton(u08 pressedButton);
void puzzleCycle(void);
void puzzleInit(u08 mode);
#endif
