%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define YYSTYPE double

// The resulting expression value
// NOTE: All calculations are done in type double for accuracy. The
// resulting value of the expression is returned as int of this double.
int exprValue;

// Indicates if an error occured while evaluating a variable (most
// likely caused by variable not being active or invalid identifier)
extern int varError;

// Prototype of generated scanner functions in expr.yy.c
struct yy_buffer_state *yy_scan_string(const char[]);
void yy_delete_buffer (struct yy_buffer_state *b);
int yywrap (void);
%}

%token NUMBER
%token PLUS MINUS TIMES DIVIDE MODULO
%token LEFT RIGHT
%token END
%token UNKNOWN

%left PLUS MINUS
%left TIMES DIVIDE MODULO
%left NEG

%start Input
%%

Input:
    | Input Line
;

Line:
    END
    | Expression END { exprValue = (int)$1;
        /*printf("Result double: %f\n", $1);
        printf("Result int   : %d\n", (int)$1);
        printf("varError     : %d\n", varError);*/ }
;

Expression:
    NUMBER { $$ = $1; }
    | Expression PLUS Expression { $$ = $1 + $3; }
    | Expression MINUS Expression { $$ = $1 - $3; }
    | Expression TIMES Expression { $$ = $1 * $3; }
    | Expression DIVIDE Expression { $$ = $1 / $3; }
    | Expression MODULO Expression { if ((int)($3) == 0) { $$ = -2.1417483647E11; } else { $$ = (double)((int)($1) % (int)($3)); } }
    | MINUS Expression %prec NEG { $$ = -$2; }
    | LEFT Expression RIGHT { $$ = $2; }
    | UNKNOWN { YYERROR; }
;

%%
int yyerror(char *s)
{
  //printf("%s\n", s);
  return 0;
}

int exprEvaluate(char *expString)
{
  struct yy_buffer_state *buf;
  char yyInput[strlen(expString) + 1];
  int retVal = 0;

  // Uncomment this to get bison runtime trace output.
  // This requires the bison compiler to use the --debug option.
  // For this, uncomment flag BDEBUG in MakefileEmu [firmware].
  // Keep the code section below and that build flag in sync.
  //extern int yydebug;
  //yydebug = 1;

  // Reset previous error
  varError = 0;

  // Copy input as scan/parse may alter original string
  strcpy(yyInput, expString);

  // Scan the expression
  buf = yy_scan_string(yyInput);

  // Parse the expression
  retVal = yyparse();

  // Cleanup
  yy_delete_buffer(buf);

  return retVal;
}

