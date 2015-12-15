/******************************************************************************
 * Filename : 'expr.l'
 * Title    : Lexical scanner for mchron expression evaluator
 ******************************************************************************/

%{
#define YYSTYPE double
#include <stdlib.h>
#include "expr.tab.h"
#include "listvarutil.h"
%}

white [ \t]+
digit [0-9]
integer {digit}+
exponent [e][+-]?{integer}
double {integer}("."{integer})?{exponent}?
var [a-zA-Z]+

%%

{white} { }

{double} { yylval = atof(yytext); return NUMBER; }
"false" { yylval = 0; return NUMBER; }
"true"	{ yylval = 1; return NUMBER; }
"null"	{ yylval = 0; return NUMBER; }
"pi"	{ yylval = M_PI; return NUMBER; }

"+"	{ return PLUS; }
"-"	{ return MINUS; }
"*"	{ return TIMES; }
"/"	{ return DIVIDE; }
"%"	{ return MODULO; }
"^"	{ return POWER; }

"&&"	{ return AND; }
"||"	{ return OR; }

">="	{ return GET; }
"<="	{ return LET; }
">"	{ return GT; }
"<"	{ return LT; }
"=="	{ return EQ; }
"!="	{ return NEQ; }

"abs"	{ return ABS; }
"cos"	{ return COS; }
"frac"	{ return FRAC; }
"int"	{ return INT; }
"sin"	{ return SIN; }

"("	{ return LEFT; }
")"	{ return RIGHT; }

"?"	{ return QMARK; }
":"	{ return COLON; }

"="	{ return IS; }

{var}	{ yylval = (double)varIdGet(yytext);
          if (yylval >= 0)
            return VARIABLE;
          varError = 2;
          return UNKNOWN; }

"\n"	{ return END; }
.	{ return UNKNOWN; }

