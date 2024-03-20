//*****************************************************************************
// Filename : 'varutil.c'
// Title    : Named variable utility routines for emuchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <regex.h>

// Monochron and emuchron defines
#include "../global.h"
#include "mchronutil.h"
#include "scanutil.h"
#include "varutil.h"

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

// Variable printing spacing (in characters)
#define VAR_WIDTH_VAR		14
#define VAR_WIDTH_LINE		70

// A structure to hold runtime information for a named numeric variable
typedef struct _varVariable_t
{
  char *varName;		// Variable name
  u08 active;			// Whether variable is in use
  double varValue;		// The current numeric value of the variable
  struct _varVariable_t *prev;	// Pointer to preceding bucket member
  struct _varVariable_t *next;	// Pointer to next bucket member
} varVariable_t;

// A structure to hold a list of numeric variables
typedef struct _varBucket_t
{
  int count;			// Number of bucket members
  struct _varVariable_t *var;	// Pointer to first bucket member
} varBucket_t;

// Create the variable buckets and also administer the total number of
// bucket members (=variables) in use
static varBucket_t varBucket[VAR_BUCKETS];
static int varCount = 0;

// Local function prototypes
static u08 varClear(int varId);

//
// Function: varClear
//
// Clear a variable using its id
//
static u08 varClear(int varId)
{
  int bucketId;
  int bucketListId;
  int i = 0;
  u08 retVal = CMD_RET_OK;
  varVariable_t *delVar;

  // Get reference to variable
  bucketId = varId & VAR_BUCKETS_MASK;
  bucketListId = (varId >> VAR_BUCKETS_BITS);
  if (bucketListId > varBucket[bucketId].count)
    emuCoreDump(CD_VAR, __func__, bucketId, bucketListId,
      varBucket[bucketId].count, 0);
  delVar = varBucket[bucketId].var;
  for (i = 0; i < bucketListId; i++)
    delVar = delVar->next;

  // If the variable is inactive it is an error (but we'll remove anyway)
  if (delVar->active == MC_FALSE)
    retVal = CMD_RET_ERROR;

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
  free(delVar->varName);
  free(delVar);

  return retVal;
}

//
// Function: varIdGet
//
// Get the id of a named variable using its name. When the name is scanned by
// the flex lexer it is guaranteed to consist of any combination of [a-zA-Z_]
// characters. When used in commmands 'lr'/'tg'/'vr' the command line scanner
// and command handler functions are responsible for checking whether the var
// name consists of [a-zA-Z_] chars.
// Argument create defines whether to create new id when the var name is not
// found.
// Return values:
// >=0 : variable id (combination of bucket id and index in bucket)
//  -1 : variable bucket overflow occurred while attempting to create new id
//  -2 : variable not found and no new id is created
//
int varIdGet(char *varName, u08 create)
{
  int bucketId;
  int bucketListId;
  u08 found = MC_FALSE;
  int compare = 0;
  int i = 0;
  varVariable_t *checkVar;
  varVariable_t *myVar;

  // Create a simple hash of first and optionally second character of var name
  // to obtain a variable bucket number
  if (varName[0] < 'a')
    bucketId = varName[0] - 'A';
  else
    bucketId = varName[0] - 'a';
  if (varName[1] != '\0')
  {
    if (varName[1] < 'a')
      bucketId = bucketId + (varName[1] - 'A');
    else
      bucketId = bucketId + (varName[1] - 'a');
  }
  while (bucketId >= VAR_BUCKETS)
    bucketId = bucketId - VAR_BUCKETS;

  // Find the variable in the bucket
  checkVar = varBucket[bucketId].var;
  while (i < varBucket[bucketId].count && found == MC_FALSE)
  {
    compare = strcmp(checkVar->varName, varName);
    if (compare == 0)
    {
      found = MC_TRUE;
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

  // Determine search result
  if (found == MC_TRUE)
  {
    // Var name found
    bucketListId = i;
  }
  else if (create == MC_FALSE)
  {
    // Var name not found
    return -2;
  }
  else
  {
    // Var name not found in the bucket so let's add it. However, first check
    // for bucket overflow.
    if (varBucket[bucketId].count == VAR_BUCKET_SIZE_COUNT)
    {
      printf("cannot register variable: %s\n", varName);
      return -1;
    }

    // Add variable to end of the bucket list
    myVar = malloc(sizeof(varVariable_t));
    myVar->varName = malloc(strlen(varName) + 1);
    strcpy(myVar->varName, varName);
    myVar->active = MC_FALSE;
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
  //  VAR_BUCKETS_MASK, VAR_BUCKET_SIZE_COUNT);
}

//
// Function: varPrint
//
// Print the value of named variables using a regex pattern (where '.' matches
// every named variable)
//
u08 varPrint(char *pattern, u08 summary)
{
  regex_t regex;
  int status = 0;
  int spaceCount = 0;
  int varInUse = 0;
  int i;
  int varIdx = 0;
  u08 allSorted = MC_FALSE;

  // Validate regex pattern
  if (regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB) != 0)
  {
    regfree(&regex);
    return CMD_RET_ERROR;
  }

  if (varCount != 0)
  {
    // Get pointers to all variables and then sort them
    varVariable_t *myVar;
    varVariable_t *varSort[varCount];

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
    while (allSorted == MC_FALSE)
    {
      allSorted = MC_TRUE;
      for (i = 0; i < varIdx - 1; i++)
      {
        if (strcmp(varSort[i]->varName, varSort[i + 1]->varName) > 0)
        {
          myVar = varSort[i];
          varSort[i] = varSort[i + 1];
          varSort[i + 1] = myVar;
          allSorted = MC_FALSE;
        }
      }
      varIdx--;
    }

    // Print the vars from the sorted array
    for (i = 0; i < varCount; i++)
    {
      if (varSort[i]->active == MC_TRUE)
      {
        // See if variable name matches regex pattern
        status = regexec(&regex, varSort[i]->varName, (size_t)0, NULL, 0);
        if (status == 0)
        {
          varInUse++;
          spaceCount = spaceCount + printf("%s=", varSort[i]->varName) +
            cmdArgValuePrint(varSort[i]->varValue, MC_FALSE, MC_FALSE);
          if (spaceCount % VAR_WIDTH_VAR != 0 && spaceCount < VAR_WIDTH_LINE)
          {
            printf("%*s", VAR_WIDTH_VAR - spaceCount % VAR_WIDTH_VAR, "");
            spaceCount = spaceCount + VAR_WIDTH_VAR -
              spaceCount % VAR_WIDTH_VAR;
          }
          if (spaceCount >= VAR_WIDTH_LINE)
          {
            spaceCount = 0;
            printf("\n");
          }
        }
      }
    }
  }

  // End on newline if needed and provide variable summary
  if (spaceCount != 0)
    printf("\n");
  if (summary == MC_TRUE && varInUse != 1)
    printf("registered variables: %d\n", varInUse);

  // Cleanup regex
  regfree(&regex);

  return CMD_RET_OK;
}

//
// Function: varReset
//
// Reset all named variable data
//
int varReset(void)
{
  int i = 0;
  int varInUse = 0;
  varVariable_t *delVar;
  varVariable_t *nextVar;

  // Clear each bucket
  for (i = 0; i < VAR_BUCKETS; i++)
  {
    // Clear all variables in bucket
    nextVar = varBucket[i].var;
    while (nextVar != NULL)
    {
      delVar = nextVar;
      nextVar = delVar->next;
      if (delVar->active == MC_TRUE)
        varInUse++;
      free(delVar->varName);
      free(delVar);
    }
    varBucket[i].count = 0;
    varBucket[i].var = NULL;
  }
  varCount = 0;

  return varInUse;
}

//
// Function: varResetVar
//
// Clear a variable using a variable name
//
u08 varResetVar(char *varName)
{
  int varId;

  // Clear the single variable
  varId = varIdGet(varName, MC_FALSE);
  if (varId < 0)
    return CMD_RET_ERROR;
  return varClear(varId);
}

//
// Function: varValGet
//
// Get the value of a named variable using its id
//
double varValGet(int varId, u08 *varStatus)
{
  int bucketId;
  int bucketListId;
  int i = 0;
  varVariable_t *myVar;

  // Check if we have a valid id
  if (varId < 0)
  {
    *varStatus = VAR_NOTINUSE;
    return 0;
  }

  // Get reference to variable
  bucketId = varId & VAR_BUCKETS_MASK;
  bucketListId = (varId >> VAR_BUCKETS_BITS);
  if (bucketListId > varBucket[bucketId].count)
    emuCoreDump(CD_VAR, __func__, bucketId, bucketListId,
      varBucket[bucketId].count, 0);
  myVar = varBucket[bucketId].var;
  for (i = 0; i < bucketListId; i++)
    myVar = myVar->next;

  // Only an active variable has a value
  if (myVar->active == 0)
  {
    printf("variable not in use: %s\n", myVar->varName);
    *varStatus = VAR_NOTINUSE;
    return 0;
  }

  // Return value
  *varStatus = VAR_OK;
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
    varVariable_t *myVar;

    // Get reference to variable
    bucketId = varId & VAR_BUCKETS_MASK;
    bucketListId = (varId >> VAR_BUCKETS_BITS);
    if (bucketListId > varBucket[bucketId].count)
      emuCoreDump(CD_VAR, __func__, bucketId, bucketListId,
        varBucket[bucketId].count, 0);
    myVar = varBucket[bucketId].var;
    for (i = 0; i < bucketListId; i++)
      myVar = myVar->next;

    // Make variable active (if not already) and assign value
    myVar->active = MC_TRUE;
    myVar->varValue = value;
  }

  return value;
}
