//*****************************************************************************
// Filename : 'mchron.h'
// Title    : Definitions for mchron command handler routines
//*****************************************************************************

#ifndef MCHRON_H
#define MCHRON_H

#include "interpreter.h"

// Standard command handler function prototypes
int doAlarmPos(cmdLine_t *cmdLine);
int doAlarmSet(cmdLine_t *cmdLine);
int doAlarmToggle(cmdLine_t *cmdLine);
int doBeep(cmdLine_t *cmdLine);
int doClockFeed(cmdLine_t *cmdLine);
int doClockSelect(cmdLine_t *cmdLine);
int doComments(cmdLine_t *cmdLine);
int doDateSet(cmdLine_t *cmdLine);
int doDateReset(cmdLine_t *cmdLine);
int doExecute(cmdLine_t *cmdLine);
int doHelp(cmdLine_t *cmdLine);
int doHelpCmd(cmdLine_t *cmdLine);
int doHelpExpr(cmdLine_t *cmdLine);
int doLcdBacklightSet(cmdLine_t *cmdLine);
int doLcdErase(cmdLine_t *cmdLine);
int doLcdInverse(cmdLine_t *cmdLine);
int doMonochron(cmdLine_t *cmdLine);
int doPaintAscii(cmdLine_t *cmdLine);
int doPaintCircle(cmdLine_t *cmdLine);
int doPaintCircleFill(cmdLine_t *cmdLine);
int doPaintDot(cmdLine_t *cmdLine);
int doPaintLine(cmdLine_t *cmdLine);
int doPaintNumber(cmdLine_t *cmdLine);
int doPaintRect(cmdLine_t *cmdLine);
int doPaintRectFill(cmdLine_t *cmdLine);
int doStatsPrint(cmdLine_t *cmdLine);
int doStatsReset(cmdLine_t *cmdLine);
int doTimeFlush(cmdLine_t *cmdLine);
int doTimePrint(cmdLine_t *cmdLine);
int doTimeReset(cmdLine_t *cmdLine);
int doTimeSet(cmdLine_t *cmdLine);
int doVarPrint(cmdLine_t *cmdLine);
int doVarReset(cmdLine_t *cmdLine);
int doVarSet(cmdLine_t *cmdLine);
int doWait(cmdLine_t *cmdLine);
int doExit(cmdLine_t *cmdLine);

// Program counter control block handler prototypes
int doIfElse(cmdLine_t **cmdProgCounter);
int doIfElseIf(cmdLine_t **cmdProgCounter);
int doIfEnd(cmdLine_t **cmdProgCounter);
int doIfThen(cmdLine_t **cmdProgCounter);
int doRepeatFor(cmdLine_t **cmdProgCounter);
int doRepeatNext(cmdLine_t **cmdProgCounter);
#endif
