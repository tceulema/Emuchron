//*****************************************************************************
// Filename : 'dictutil.h'
// Title    : Defines for mchron command dictionary functionality
//*****************************************************************************

#ifndef DICTUTIL_H
#define DICTUTIL_H

#include "interpreter.h"

// mchron command dictionary functions
cmdCommand_t *dictCmdGet(char *cmdName);
u08 dictPrint(char *pattern);
#endif
