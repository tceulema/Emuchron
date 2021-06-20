//*****************************************************************************
// Filename : 'varutil.h'
// Title    : Defines for named variable support functionality
//*****************************************************************************

#ifndef VARUTIL_H
#define VARUTIL_H

#include "../avrlibtypes.h"

// The variable status types
#define VAR_OK		0
#define VAR_NOTINUSE	1
#define VAR_OVERFLOW	2

// mchron variable support functions
void varInit(void);
u08 varPrint(char *pattern, u08 summary);
int varReset(void);

// Functions for referencing and manipulating variables
int varIdGet(char *varName, u08 create);
u08 varClear(int varId);
double varValGet(int varId, u08 *varStatus);
double varValSet(int varId, double value);
#endif
