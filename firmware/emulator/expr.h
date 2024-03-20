//*****************************************************************************
// Filename : 'expr.h'
// Title    : Expression evaluator defs as created in flex/bison
//*****************************************************************************

#ifndef EXPR_H
#define EXPR_H

#include "../avrlibtypes.h"
#include "interpreter.h"

// Evaluate mchron numeric expression
u08 exprEvaluate(char *argName, argInfo_t *argInfo);
u08 exprVarSetU08(char *argName, char *varName, u08 value);
#endif
