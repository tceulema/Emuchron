//*****************************************************************************
// Filename : 'expr.h'
// Title    : Expression evaluator defs as created in flex/bison 
//*****************************************************************************

#ifndef EXPR_H
#define EXPR_H

// This macro makes an interface to the expression evaluator with default
// error handling
#define EXPR_EVALUATE(a,b) \
  if (exprEvaluate(a, b, strlen(b)) != CMD_RET_OK) \
    return CMD_RET_ERROR

int exprEvaluate(char *argName, char *exprString, int exprStringLen);
#endif
