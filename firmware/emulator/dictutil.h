//*****************************************************************************
// Filename : 'dictutil.h'
// Title    : Defines for mchron command dictionary functionality
//*****************************************************************************

#ifndef DICTUTIL_H
#define DICTUTIL_H

#include "../avrlibtypes.h"
#include "interpreter.h"

// The command property to search for
#define CMD_SEARCH_NAME		0
#define CMD_SEARCH_DESCR	1
#define CMD_SEARCH_ARG		2
#define CMD_SEARCH_ALL		3

// mchron command dictionary functions
cmdCommand_t *dictCmdGet(char *cmdName);
u08 dictPrint(char *pattern, u08 searchType);
u08 dictVerify(void);
#endif
