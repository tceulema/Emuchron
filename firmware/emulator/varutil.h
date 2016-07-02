//*****************************************************************************
// Filename : 'varutil.h'
// Title    : Defines for named variable support functionality
//*****************************************************************************

#ifndef VARUTIL_H
#define VARUTIL_H

// mchron variable support functions
int varClear(char *argName, char *var);
void varInit(void);
int varPrint(char *argName, char *var);
void varReset(void);

// Functions for referencing and manipulating variables
int varIdGet(char *var);
double varValGet(int varId, int *varError);
double varValSet(int varId, double value);
#endif
