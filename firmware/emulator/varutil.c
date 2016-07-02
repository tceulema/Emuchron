//*****************************************************************************
// Filename : 'varutil.c'
// Title    : Named variable utility routines for emuchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Monochron and emuchron defines
#include "../ks0108.h"
#include "interpreter.h"
#include "scanutil.h"
#include "varutil.h"

// A structure to hold runtime information for a named numeric variable
typedef struct _variable_t
{
  char *name;			// Variable name
  int active;			// Whether variable is in use
  double varValue;		// The current numeric value of the variable
  struct _variable_t *prev;	// Pointer to preceding bucket member
  struct _variable_t *next;	// Pointer to next bucket member
} variable_t;

// A structure to hold a list of numeric variables
typedef struct _varBucket_t
{
  int count;			// Number of bucket members
  struct _variable_t *var;	// Pointer to first bucket member
} varBucket_t;

// The administration of mchron variables.
// Variables are spread over buckets. There are VAR_BUCKETS buckets.
// For administering buckets we use VAR_BUCKETS_BITS bits.
// WARNING: Make sure that VAR_BUCKETS <= 2 ^ VAR_BUCKETS_BITS
// Each bucket can contain up to VAR_BUCKET_SIZE_COUNT variables.
// For administering a bucket size we use VAR_BUCKET_SIZE_BITS bits.
// A variable id is a combination of VAR_BUCKET_SIZE_BITS + VAR_BUCKETS_BITS
// bits. For portability reasons do not exceed a total length of 16 bits.
#define VAR_BUCKETS		51
#define VAR_BUCKETS_BITS	6
#define VAR_BUCKETS_MASK	(~((~(int)0) << VAR_BUCKETS_BITS))
#define VAR_BUCKET_SIZE_BITS	8
#define VAR_BUCKET_SIZE_COUNT	(~((~(int)0) << VAR_BUCKET_SIZE_BITS) + 1)

// Create the variable buckets and also administer the total number of
// bucket members (=variables) in use.
static varBucket_t varBucket[VAR_BUCKETS];
static int varCount = 0;

// Local function prototypes
static int varPrintValue(char *var, double value, int detail);

//
// Function: varClear
//
// Clear a named variable
//
int varClear(char *argName, char *var)
{
  int varId;
  int bucketId;
  int bucketListId;
  int i = 0;
  variable_t *delVar;

  varId = varIdGet(var);
  if (varId == -1)
  {
    printf("%s? internal bucket overflow\n", argName);
    return CMD_RET_ERROR;
  }

  // Get reference to variable
  bucketId = varId & VAR_BUCKETS_MASK;
  bucketListId = (varId >> VAR_BUCKETS_BITS);
  delVar = varBucket[bucketId].var;
  for (i = 0; i < bucketListId; i++)
    delVar = delVar->next;

  // Remove variable from the list
  if (delVar->prev != NULL)
    delVar->prev->next = delVar->next;
  else
    varBucket[bucketId].var = delVar->next;
  if (delVar->next != NULL)
    delVar->next->prev = delVar->prev;

  // Correct variable counters
  varBucket[bucketId].count--;
  varCount--;

  // Return the malloced variable name and the structure itself
  free(delVar->name);
  free(delVar);

  return CMD_RET_OK;
}

//
// Function: varIdGet
//
// Get the id of a named variable using its name. When the name is scanned by
// the flex lexer it is guaranteed to consist of any combination of [a-zA-Z_]
// characters. When used in commmand 'vp' or 'vr' the command handler function
// is responsible for checking whether the var name consists of [a-zA-Z_] chars
// or a '*' string.
// Return values:
// >=0 : variable id (combination of bucket id and index in bucket)
//  -1 : variable bucket overflow
//
int varIdGet(char *var)
{
  int bucketId;
  int bucketListId;
  int found = GLCD_FALSE;
  int compare = 0;
  int i = 0;
  variable_t *checkVar;
  variable_t *myVar;

  // Create a simple hash of first and optionally second character of
  // var name to obtain a variable bucket number
  if (var[0] < 'a')
    bucketId = var[0] - 'A';
  else
    bucketId = var[0] - 'a';
  if (var[1] != '\0')
  {
    if (var[1] < 'a')
      bucketId = bucketId + (var[1] - 'A');
    else
      bucketId = bucketId + (var[1] - 'a');
  }
  while (bucketId >= VAR_BUCKETS)
    bucketId = bucketId - VAR_BUCKETS;

  // Find the variable in the bucket
  checkVar = varBucket[bucketId].var;
  while (i < varBucket[bucketId].count && found == GLCD_FALSE)
  {
    compare = strcmp(checkVar->name, var);
    if (compare == 0)
    {
      found = GLCD_TRUE;
      bucketListId = i;
    }
    else
    {
      // Not done searching
      if (i != varBucket[bucketId].count - 1)
      {
        // Next list member
        checkVar = checkVar->next;
        i++;
      }
      else
      {
        // Searched all list members and did not find variable
        break;
      }
    }
  }

  if (found == GLCD_TRUE)
  {
    // Var name found
    bucketListId = i;
  }
  else
  {
    // Var name not found in the bucket so let's add it.
    // However, first check for bucket overflow.
    if (varBucket[bucketId].count == VAR_BUCKET_SIZE_COUNT)
    {
      printf("cannot register variable: %s\n", var);
      return -1;
    }

    // Add variable to end of the bucket list
    myVar = malloc(sizeof(variable_t));
    myVar->name = malloc(strlen(var) + 1);
    strcpy(myVar->name, var);
    myVar->active = GLCD_FALSE;
    myVar->varValue = 0;
    myVar->next = NULL;

    if (varBucket[bucketId].count == 0)
    {
      // Bucket is empty
      varBucket[bucketId].var = myVar;
      myVar->prev = NULL;
      bucketListId = 0;
    }
    else
    {
      // Add to end of bucket list
      checkVar->next = myVar;
      myVar->prev = checkVar;
      bucketListId = i + 1;
    }

    // Admin counters
    varBucket[bucketId].count++;
    varCount++;
  }

  // Var name is validated and its hashed bucket with bucket list index
  // is returned
  //printf("%s: bucket=%d index=%d\n", var, bucketId, bucketListId);
  return (bucketListId << VAR_BUCKETS_BITS) + bucketId;
}

//
// Function: varInit
//
// Initialize the named variable buckets
//
void varInit(void)
{
  int i;

  for (i = 0; i < VAR_BUCKETS; i++)
  {
    varBucket[i].count = 0;
    varBucket[i].var = NULL;
  }

  varCount = 0;
  //printf("\nbucket mask: %d, bucket size count: %d\n",
  //  VAR_BUCKETS_MASK ,VAR_BUCKET_SIZE_COUNT);
}

//
// Function: varPrint
//
// Print the value of a single or all named variables
//
int varPrint(char *argName, char *var)
{
  // Get and print the value of one or all variables
  if (strcmp(var, "*") == 0)
  {
    const int spaceCountMax = 60;
    int spaceCount = 0;
    int varInUse = 0;
    int i;
    int varIdx = 0;
    int allSorted = GLCD_FALSE;

    if (varCount != 0)
    {
      // Get pointers to all variables and then sort them
      variable_t *myVar;
      variable_t *varSort[varCount];

      for (i = 0; i < VAR_BUCKETS; i++)
      {
        // Copy references to all bucket list members
        myVar = varBucket[i].var;
        while (myVar != NULL)
        {
          varSort[varIdx] = myVar;
          myVar = myVar->next;
          varIdx++;
        }
      }

      // Sort the array based on var name
      while (allSorted == GLCD_FALSE)
      {
        allSorted = GLCD_TRUE;
        for (i = 0; i < varIdx - 1; i++)
        {
          if (strcmp(varSort[i]->name, varSort[i + 1]->name) > 0)
          {
            myVar = varSort[i];
            varSort[i] = varSort[i + 1];
            varSort[i + 1] = myVar;
            allSorted = GLCD_FALSE;
          }
        }
        varIdx--;
      }

      // Print the vars from the sorted array
      for (i = 0; i < varCount; i++)
      {
        if (varSort[i]->active == GLCD_TRUE)
        {
          varInUse++;
          spaceCount = spaceCount +
            varPrintValue(varSort[i]->name, varSort[i]->varValue, GLCD_FALSE);
          if (spaceCount % 10 != 0)
          {
            printf("%*s", 10 - spaceCount % 10, "");
            spaceCount = spaceCount + 10 - spaceCount % 10;
          }
          if (spaceCount >= spaceCountMax)
          {
            spaceCount = 0;
            printf("\n");
          }
        }
      }
    }

    // End on newline if needed and provide variable summary
    if (spaceCount != 0)
      printf("\n");
    printf("variables in use: %d\n", varInUse);
  }
  else
  {
    // Get and print the value of a single variable, when active
    int varId;
    double varValue = 0;
    int varError = 0;

    // Get var id
    varId = varIdGet(var);
    if (varId == -1)
    {
      printf("%s? internal bucket overflow\n", argName);
      return CMD_RET_ERROR;
    }

    // Get var value
    varValue = varValGet(varId, &varError);
    if (varError == 1)
      return CMD_RET_ERROR;

    // Print var value
    varPrintValue(var, varValue, GLCD_TRUE);
    printf("\n");
  }

  return CMD_RET_OK;
}

//
// Function: varPrintValue
//
// Print a single variable value and return the length
// of the printed string
//
static int varPrintValue(char *var, double value, int detail)
{
  return printf("%s=", var) + cmdArgValuePrint(value, detail);
}

//
// Function: varReset
//
// Reset all named variable data
//
void varReset(void)
{
  int i = 0;
  variable_t *delVar;
  variable_t *nextVar;

  // Clear each bucket
  for (i = 0; i < VAR_BUCKETS; i++)
  {
    // Clear all variable in bucket
    nextVar = varBucket[i].var;
    while (nextVar != NULL)
    {
      delVar = nextVar;
      nextVar = delVar->next;
      free(delVar->name);
      free(delVar);
    }
    varBucket[i].count = 0;
    varBucket[i].var = NULL;
  }
  varCount = 0;
}

//
// Function: varValGet
//
// Get the value of a named variable using its id
//
double varValGet(int varId, int *varError)
{
  int bucketId;
  int bucketListId;
  int i = 0;
  variable_t *myVar;

  // Check if we have a valid id
  if (varId < 0)
  {
    *varError = 1;
    return 0;
  }

  // Get reference to variable
  bucketId = varId & VAR_BUCKETS_MASK;
  bucketListId = (varId >> VAR_BUCKETS_BITS);
  myVar = varBucket[bucketId].var;
  for (i = 0; i < bucketListId; i++)
    myVar = myVar->next;

  // Only an active variable has a value
  if (myVar->active == 0)
  {
    printf("variable not in use: %s\n", myVar->name);
    *varError = 1;
    return 0;
  }

  // Return value
  *varError = 0;
  return myVar->varValue;
}

//
// Function: varValSet
//
// Set the value of a named variable using its id.
//
// In case a scanner/parser error occurs during the expression evaluation
// process we won't even get in here. However, we still need to check the
// end result value for anomalies, in this case NaN and infinite. If
// something is wrong, do not assign the value to the variable.
// Further error handling for the expression evaluator will take place in
// exprEvaluate() that will provide an error code to its caller.
//
double varValSet(int varId, double value)
{
  // Check end result of expression
  if (isnan(value) == 0 && isfinite(value) != 0)
  {
    int bucketId;
    int bucketListId;
    int i = 0;
    variable_t *myVar;

    // Get reference to variable
    bucketId = varId & VAR_BUCKETS_MASK;
    bucketListId = (varId >> VAR_BUCKETS_BITS);
    myVar = varBucket[bucketId].var;
    for (i = 0; i < bucketListId; i++)
      myVar = myVar->next;

    // Make variable active (if not already) and assign value
    myVar->active = GLCD_TRUE;
    myVar->varValue = value;
  }

  return value;
}

