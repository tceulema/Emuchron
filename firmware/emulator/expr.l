%{
#define YYSTYPE double
#include <stdlib.h>
#include "expr.tab.h"
#include "scanutil.h"

int varError = 0;

int exprVarValGet(char *var)
{
  int varValue = 0;
  int retVal = 0;
  int active = 0;

  retVal = varStateGet(var, &active);
  if (retVal != 0)
  {
    // Variable does not exist
    varError = 1;
    return 0;
  }
  else if (active == 0)
  {
    // Variable is not active
    printf("variable not in use: %s\n", var);
    varError = 1;
    return 0;
  }

  varValGet(var, &varValue);
  return varValue;
}
%}

white [ \t]+
digit [0-9]
integer {digit}+
var [a-z]+

%%

{white} { }
{integer} { yylval = atof(yytext); return NUMBER; }
"+"	{ return PLUS; }
"-"	{ return MINUS; }
"*"	{ return TIMES; }
"/"	{ return DIVIDE; }
"%"	{ return MODULO; }
"("	{ return LEFT; }
")"	{ return RIGHT; }
"\n"	{ return END; }
{var}	{ yylval = (double)exprVarValGet(yytext); if (varError == 0) return NUMBER; }
.	{ return UNKNOWN; }

