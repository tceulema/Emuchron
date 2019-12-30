//*****************************************************************************
// Filename : 'expr.h'
// Title    : Expression evaluator defs as created in flex/bison
//*****************************************************************************

#ifndef EXPR_H
#define EXPR_H

#include "interpreter.h"

// Evaluate mchron numeric expression
u08 exprEvaluate(char *argName, char *exprString);
#endif
