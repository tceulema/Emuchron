//*****************************************************************************
// Filename : 'expr.y'
// Title    : Parser for mchron expression evaluator
//*****************************************************************************

%{
// All calculations in bison are done in type double
#define YYSTYPE double

// The following expression evaluator objects are the only ones that are
// externally accessible
double exprValue;		// The resulting expression value
unsigned char exprAssign;	// Indicates if input expression is an assignment

// Variable error status flag (variable is not active)
static int varError;

// Dummy value to be used in c library math functions
static double myDummy;

// Prototypes of expr.yy.c local functions. They are either copied from
// within this module or are bison generated parser functions. They are
// added to satisfy gcc.
static double exprCompare(double valLeft, double valRight, int cond);
static struct yy_buffer_state *yy_scan_string(const char[]);
static void yy_delete_buffer(struct yy_buffer_state *b);
static int yyerror(char *s);
static int yylex(void);

// The comparison logic conditions in exprCompare()
#define COND_LT		0	// <
#define COND_GT		1	// >
#define COND_LET	2	// <=
#define COND_GET	3	// >=
#define COND_NEQ	4	// !=
#define COND_EQ		5	// ==

// The relative accuracy of comparing values being equal in exprCompare().
// Current value 1E-7L is considered to provide a wide margin of error,
// but for our mchron purpose it is accurate enough.
#define EPSILON		1E-7L

//
// Function: exprCompare
//
// Compare two double type values using a comparison condition.
// It is partly based on information in the following web resource:
// http://warp.povusers.org/programming/floating_point_caveats.html
//
// When comparing type double values, the slightest inaccuracy in the
// mantisse will cause undesired comparison results. This is an inherent
// issue for non-integer data types, We will therefor compare values
// based on their relative delta using accuracy cutoff value EPSILON.
//
// Returns: 0 - comparison condition fails
//          1 - comparison condition passes
//
static double exprCompare(double valLeft, double valRight, int cond)
{
  int isEqual = 0;

  // Determine if left and right values are considered equal
  if (valLeft == 0 || valRight == 0)
  {
    // In case valLeft is 0: cannot divide by zero as done in 'else' branch
    // so we have to use the comparison expression in this section.
    // In case valRight is 0: yes, we could compare based on relative delta
    // in the 'else' branch, but we won't. Think about swapping the left and
    // right values: comparing (0 vs <low number>) is likely to yield a
    // different result than when comparing (<low number> vs 0).
    // Example:
    // Using logic in this 'then' branch: (0 == 1e-16) --> true
    // Using logic in the 'else' branch : (1e-16 == 0) --> false
    // That is nasty!
    // Hence, in case either one of the left or right values is 0, always
    // use the expression below for consistent comparison results.
    if (fabs(valLeft - valRight) < EPSILON)
      isEqual = 1;
  }
  else
  {
    // Determine relative delta between the left and right value
    // and compare that with cutoff value epsilon
    if (fabs((valLeft - valRight) / valLeft) < EPSILON)
      isEqual = 1;
  }

  // Now we know whether the left and right values are considered equal,
  // determine the boolean result of the comparison
  if (isEqual == 1)
  {
    // Based on the fact that the two values are considered equal
    if (cond == COND_LET || cond == COND_GET || cond == COND_EQ)
      return 1;
    else
      return 0;
  }
  else
  {
    // Based on the fact that the two values are considered unequal
    if (cond == COND_NEQ)
      return 1;
    else if (valLeft > valRight && (cond == COND_GT || cond == COND_GET))
      return 1;
    else if (valLeft < valRight && (cond == COND_LT || cond == COND_LET))
      return 1;
    else
      return 0;
  }
}
%}

// The supported tokens from flex
%token NUMBER VARIABLE IS
%token QMARK COLON
%token PLUS MINUS TIMES DIVIDE MODULO POWER
%token SIN COS ABS FRAC INTEGER RAND
%token AND OR
%token BITAND BITOR BITNOT SHIFTL SHIFTR
%token LET GET LT GT EQ NEQ
%token LEFT RIGHT
%token END
%token UNKNOWN

// Expression evaluation operator precedence, ranking from low to high, that
// is in line with https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B
// There is one exception, being the POWER token that does not exist in 'C'.
%left IS
%left QMARK COLON
%left OR
%left AND
%left BITOR
%left BITAND
%left EQ NEQ
%left LET GET LT GT
%left SHIFTL SHIFTR
%left PLUS MINUS BITNOT
%left TIMES DIVIDE MODULO
%left POWER
%left NEG
%left SIN COS ABS FRAC INTEGER RAND

// Our bison parser
%start Input
%%

Input:
    | Input Line
;

Line:
    END
    | Assignment END { exprValue = $1;
        /*printf("Result   : %f\n", $1);
        printf("varError : %d\n", varError);*/ }
    | Expression END { exprValue = $1;
        /*printf("Result   : %f\n", $1);
        printf("varError : %d\n", varError);*/ }
;

Assignment:
    // Assignment expression to set value of variable
    VARIABLE IS Expression { $$ = varValSet((int)$1, $3); exprAssign = 1; }
;

Expression:
    // Fixed input number or constant (pi/true/false/null)
    NUMBER { $$ = $1; }
    // Get variable value
    | VARIABLE { $$ = varValGet((int)$1, &varError);
        if (varError == 1) { YYERROR; } }
    // Mathematical operator expressions
    | Expression PLUS Expression { $$ = $1 + $3; }
    | Expression MINUS Expression { $$ = $1 - $3; }
    | Expression TIMES Expression { $$ = $1 * $3; }
    | Expression DIVIDE Expression { $$ = $1 / $3; }
    | Expression MODULO Expression { $$ = fmod($1, $3); }
    | Expression POWER Expression { $$ = pow($1, $3); }
    // Ternary conditional expression
    | Expression QMARK Expression COLON Expression { $$ = $1 ? $3 : $5; }
    // Negate expression
    | MINUS Expression %prec NEG { $$ = -$2; }
    // Parenthesis enclosed expression influencing precedence of evaluation
    | LEFT Expression RIGHT { $$ = $2; }
    // Mathematical function expressions
    | ABS LEFT Expression RIGHT { $$ = fabs($3); }
    | COS LEFT Expression RIGHT { $$ = cos($3); }
    | FRAC LEFT Expression RIGHT { $$ = modf($3, &myDummy); }
    | INTEGER LEFT Expression RIGHT { modf($3, &($$)); }
    | RAND LEFT RIGHT { $$ = (double)rand() / RAND_MAX; }
    | RAND LEFT Expression RIGHT
      { if ($3 >= 1)
          srand((int)$3);
        else
          srand(time(NULL));
        $$ = (double)rand() / RAND_MAX; }
    | SIN LEFT Expression RIGHT { $$ = sin($3); }
    // Bit operators
    // Note: Bit operations are done on type unsigned int
    | Expression BITAND Expression
      { $$ = (double)((unsigned int)$1 & (unsigned int)$3); }
    | Expression BITOR Expression
      { $$ = (double)((unsigned int)$1 | (unsigned int)$3); }
    | BITNOT Expression { $$ = (double)(~((unsigned int)$2)); }
    | Expression SHIFTL Expression
      { $$ = (double)((unsigned int)$1 << (unsigned int)$3); }
    | Expression SHIFTR Expression
      { $$ = (double)((unsigned int)$1 >> (unsigned int)$3); }
    // Boolean logic expressions
    // Note that function exprCompare() is defined at the top of this file
    | Expression AND Expression { $$ = $1 && $3; }
    | Expression OR Expression { $$ = $1 || $3; }
    | Expression GT Expression { $$ = exprCompare($1, $3, COND_GT); }
    | Expression LT Expression { $$ = exprCompare($1, $3, COND_LT); }
    | Expression GET Expression { $$ = exprCompare($1, $3, COND_GET); }
    | Expression LET Expression { $$ = exprCompare($1, $3, COND_LET); }
    | Expression EQ Expression { $$ = exprCompare($1, $3, COND_EQ); }
    | Expression NEQ Expression { $$ = exprCompare($1, $3, COND_NEQ); }
    // Can't handle unknown stuff so it should fail
    | UNKNOWN { YYERROR; }
;

%%
// Generic parser error handling
static int yyerror(char *s)
{
  //printf("%d %s\n", varError, s);
  return 0;
}

//
// Function: exprEvaluate
//
// The entry point to the generated flex/bison expression evaluator
//
int exprEvaluate(char *argName, char *exprString, int exprStringLen)
{
  struct yy_buffer_state *buf;
  char yyInput[exprStringLen + 1];
  int parseResult;

  // Uncomment this to get bison runtime trace output.
  // This requires the bison compiler to use the --debug option.
  // For this, uncomment flag BDEBUG in MakefileEmu [firmware].
  // Keep the code section below and that build flag in sync.
  //extern int yydebug;
  //yydebug = 1;

  // Reset previous error and assignment expression indicator
  varError = 0;
  exprAssign = 0;

  // Copy input as scan/parse may alter original string
  strcpy(yyInput, exprString);

  // Scan the expression
  buf = yy_scan_string(yyInput);

  // Parse the expression
  parseResult = yyparse();

  // Cleanup (including returning malloc-ed memory during scan/parse)
  yy_delete_buffer(buf);

  // Handle all kinds of erroneous end result situations
  if (varError == 1)
  {
    // Inactive variable
    printf("%s? parse error\n", argName);
    return CMD_RET_ERROR;
  }
  else if (varError == 2)
  {
    // Variable bucket overflow
    printf("%s? internal: bucket overflow\n", argName);
    return CMD_RET_ERROR;
  }
  else if (parseResult == 1)
  {
    // Error occured in scanning/parsing the expression string
    printf("%s? syntax error\n", argName);
    return CMD_RET_ERROR;
  }
  else if (isnan(exprValue) != 0)
  {
    // Result is not a number
    printf("%s? invalid (NaN)\n", argName);
    return CMD_RET_ERROR;
  }
  else if (isfinite(exprValue) == 0)
  {
    // Result is infinite
    printf("%s? overflow (inf)\n", argName);
    return CMD_RET_ERROR;
  }

  return CMD_RET_OK;
}
