//*****************************************************************************
// Filename : 'varutil.h'
// Title    : Defines for named variable support functionality
//*****************************************************************************

#ifndef VARUTIL_H
#define VARUTIL_H

// mchron variable support functions
void varInit(void);
int varPrint(char *pattern, int silent);
void varReset(void);

// Functions for referencing and manipulating variables
int varIdGet(char *var, int create);
int varClear(int varId);
double varValGet(int varId, int *varError);
double varValSet(int varId, double value);
#endif
