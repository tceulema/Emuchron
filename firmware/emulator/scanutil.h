//*****************************************************************************
// Filename : 'scanutil.h'
// Title    : Defines for command line scan and repeat variable functionality
//*****************************************************************************

#ifndef SCANUTIL_H
#define SCANUTIL_H

// The command argument profile types
#define ARG_CHAR	0
#define ARG_WORD	1
#define ARG_UINT	2
#define ARG_INT		3
#define ARG_STRING	4
#define ARG_END		5

// The argument value domain validation types
#define DOM_CHAR_LIST	0
#define DOM_WORD_LIST	1
#define DOM_NUM_RANGE	2
#define DOM_NUM_MAX	3
#define DOM_NUM_MIN	4

// The command input read methods
#define CMD_INPUT_READLINELIB	0
#define CMD_INPUT_MANUAL	1

// The command echo options
#define CMD_ECHO_NO		0
#define CMD_ECHO_YES		1
#define CMD_ECHO_INHERIT	2

// The command return values
#define CMD_RET_OK		0
#define CMD_RET_EXIT		1
#define CMD_RET_ERROR		-1
#define CMD_RET_ERROR_RECOVER	-2
#define CMD_RET_INTERRUPT	-3

// To prevent infinite looping define a max file depth when executing
// command files
#define CMD_FILE_DEPTH_MAX	8

// The command repeat types
#define CMD_REPEAT_NONE		0
#define CMD_REPEAT_WHILE	1
#define CMD_REPEAT_NEXT		2

// The repeat while end conditions
#define RU_NONE			0
#define RU_LT			1
#define RU_GT			2
#define RU_LET			3
#define RU_GET			4
#define RU_NEQ			5

// The max number of mchron command line arguments per argument type
#define ARG_TYPE_COUNT_MAX	10

// This macro makes an interface towards argScan() so we cannot make
// a mistake in the argCount parameter or its default error handling
#define _argScan(a,b) \
  if (argScan(a,sizeof(a)/sizeof(argItem_t),b,GLCD_FALSE) != CMD_RET_OK) \
    return CMD_RET_ERROR

// Definition of a structure holding the parameters for reading input
// lines from an input stream
typedef struct _cmdInput_t
{
  FILE *file;		// Input stream
  char *input;		// Pointer to resulting single input line
  int readMethod;	// Indicator for input read method
} cmdInput_t;

// Definition of a structure to hold a command line argument item
typedef struct _argItem_t
{
  int argType;					// Argument type
  char *argName;				// Argument name
  struct _argItemDomain_t *argItemDomain;	// Argument domain
} argItem_t;

// Definition of a structure to hold domain info for an argument item
typedef struct _argItemDomain_t
{
  int argDomainType;			// Argument domain type
  char *argTextList;			// Char/word argument value list 
  int argNumMin;			// Numeric argument min value
  int argNumMax;			// Numeric argument max value
} argItemDomain_t;

// Definition of a structure holding a single text line in a command file
// or an interactive command line entry action
typedef struct _cmdLine_t
{
  int lineNum;				// Line number
  char *input;				// The malloc-ed command from file or prompt
  int cmdRepeatType;			// The repeat command type	
  struct _cmdRepeat_t *cmdRepeat;	// Pointer to associated repeat data
  struct _cmdLine_t *next;		// Pointer to next list element
} cmdLine_t;

// Definition of a strucure holding a repeat next loop definition
typedef struct _cmdRepeat_t
{
  int active;				// Is repeat loop active
  int initialized;			// Are repeat loop arguments initialized
  char var[3];				// Associated variable
  int condition;			// Repeat end condition
  int start;				// Repeat start value
  char *end;				// Repeat end malloc-ed value expression
  char *step;				// Repeat step malloc-ed value expression
  struct _cmdLine_t *cmdLineRw;		// Pointer to associated repeat while command
  struct _cmdLine_t *cmdLineRn;		// Pointer to associated repeat next command
  struct _cmdRepeat_t *prev;		// Pointer to previous list element
  struct _cmdRepeat_t *next;		// Pointer to next list element
} cmdRepeat_t;

// Mchron input stream reader functions
void cmdInputInit(cmdInput_t *cmdInput);
void cmdInputRead(char *prompt, cmdInput_t *cmdInput);
void cmdInputCleanup(cmdInput_t *cmdInput);

// Mchron command line argument scanning
void argInit(char **input);
int argScan(argItem_t argItem[], int argCount, char **input, int silent);
void argRepeatItemInit(char **repeatItem, char *argExpr);

// Mchron command and repeat list support functions
int cmdFileLoad(cmdLine_t **cmdLineRoot, cmdRepeat_t **cmdRepeatRoot,
  char *fileName);
int cmdKeyboardLoad(cmdLine_t **cmdLineRoot, cmdRepeat_t **cmdRepeatRoot,
  cmdInput_t *cmdInput);
void cmdListCleanup(cmdLine_t *cmdLineRoot, cmdRepeat_t *cmdRepeatRoot);

// Variable support functions
int varClear(char *var);
void varReset(void);
int varStateGet(char *var, int *active);
int varValGet(char *var, int *value);
int varValSet(char *var, int value);
#endif

