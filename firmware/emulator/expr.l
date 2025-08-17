/******************************************************************************
 * Filename : 'expr.l'
 * Title    : Lexical scanner for mchron expression evaluator
 ******************************************************************************/

%{
#define YYSTYPE double
#include "expr.tab.h"
%}

white [ \t]+
digit [0-9]
integer {digit}+
exponent e[+-]?{integer}
double {integer}("."{integer})?{exponent}?
hex 0x[a-fA-F0-9]+
var [a-zA-Z_]+

%%

{white} { }

{double} { yylval = atof(yytext); return NUMBER; }
{hex}	{ yylval = (double)strtol(yytext, NULL, 16); return NUMBER; }
"false" { yylval = 0; return NUMBER; }
"true"	{ yylval = 1; return NUMBER; }
"null"	{ yylval = 0; return NUMBER; }
"pi"	{ yylval = M_PI; return NUMBER; }

"+"	{ return PLUS; }
"-"	{ return MINUS; }
"*"	{ return MULT; }
"/"	{ return DIVIDE; }
"%"	{ return MODULO; }
"^"	{ return POWER; }

"&&"	{ return AND; }
"||"	{ return OR; }
"!"	{ return NOT; }

"&"	{ return BITAND; }
"|"	{ return BITOR; }
"~"	{ return BITNOT; }
">>"	{ return SHIFTR; }
"<<"	{ return SHIFTL; }

">="	{ return GET; }
"<="	{ return LET; }
">"	{ return GT; }
"<"	{ return LT; }
"=="	{ return EQ; }
"!="	{ return NEQ; }

"abs"	{ return ABS; }
"cos"	{ return COS; }
"frac"	{ return FRAC; }
"int"	{ return INTEGER; }
"rand"	{ return RAND; }
"round"	{ return ROUND; }
"sin"	{ return SIN; }

"("	{ return LEFT; }
")"	{ return RIGHT; }

"?"	{ return QMARK; }
":"	{ return COLON; }

"="	{ return IS; }

{var}	{ yylval = (double)varIdGet(yytext, MC_TRUE);
          if (yylval >= 0)
            return IDENTIFIER;
          varStatus = VAR_OVERFLOW;
          return UNKNOWN; }

"\n"	{ return END; }
.	{ return UNKNOWN; }
