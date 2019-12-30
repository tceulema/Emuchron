//*****************************************************************************
// Filename : 'mchron.h'
// Title    : Definitions for mchron command handler routines
//*****************************************************************************

#ifndef MCHRON_H
#define MCHRON_H

#include "interpreter.h"

// Standard command handler function prototypes
u08 doAlarmPos(cmdLine_t *cmdLine);
u08 doAlarmSet(cmdLine_t *cmdLine);
u08 doAlarmToggle(cmdLine_t *cmdLine);
u08 doBeep(cmdLine_t *cmdLine);
u08 doClockFeed(cmdLine_t *cmdLine);
u08 doClockSelect(cmdLine_t *cmdLine);
u08 doComments(cmdLine_t *cmdLine);
u08 doDateSet(cmdLine_t *cmdLine);
u08 doDateReset(cmdLine_t *cmdLine);
u08 doExecute(cmdLine_t *cmdLine);
u08 doHelp(cmdLine_t *cmdLine);
u08 doHelpCmd(cmdLine_t *cmdLine);
u08 doHelpExpr(cmdLine_t *cmdLine);
u08 doHelpMsg(cmdLine_t *cmdLine);
u08 doLcdBacklightSet(cmdLine_t *cmdLine);
u08 doLcdCursorSet(cmdLine_t *cmdLine);
u08 doLcdCursorReset(cmdLine_t *cmdLine);
u08 doLcdDisplaySet(cmdLine_t *cmdLine);
u08 doLcdErase(cmdLine_t *cmdLine);
u08 doLcdGlutGrSet(cmdLine_t *cmdLine);
u08 doLcdHlReset(cmdLine_t *cmdLine);
u08 doLcdHlSet(cmdLine_t *cmdLine);
u08 doLcdInverse(cmdLine_t *cmdLine);
u08 doLcdNcurGrSet(cmdLine_t *cmdLine);
u08 doLcdPrint(cmdLine_t *cmdLine);
u08 doLcdRead(cmdLine_t *cmdLine);
u08 doLcdStartLineSet(cmdLine_t *cmdLine);
u08 doLcdWrite(cmdLine_t *cmdLine);
u08 doMonochron(cmdLine_t *cmdLine);
u08 doPaintAscii(cmdLine_t *cmdLine);
u08 doPaintCircle(cmdLine_t *cmdLine);
u08 doPaintCircleFill(cmdLine_t *cmdLine);
u08 doPaintDot(cmdLine_t *cmdLine);
u08 doPaintLine(cmdLine_t *cmdLine);
u08 doPaintNumber(cmdLine_t *cmdLine);
u08 doPaintRect(cmdLine_t *cmdLine);
u08 doPaintRectFill(cmdLine_t *cmdLine);
u08 doStatsPrint(cmdLine_t *cmdLine);
u08 doStatsReset(cmdLine_t *cmdLine);
u08 doTimeFlush(cmdLine_t *cmdLine);
u08 doTimePrint(cmdLine_t *cmdLine);
u08 doTimeReset(cmdLine_t *cmdLine);
u08 doTimeSet(cmdLine_t *cmdLine);
u08 doVarPrint(cmdLine_t *cmdLine);
u08 doVarReset(cmdLine_t *cmdLine);
u08 doVarSet(cmdLine_t *cmdLine);
u08 doWait(cmdLine_t *cmdLine);
u08 doWaitTimerExpiry(cmdLine_t *cmdLine);
u08 doWaitTimerStart(cmdLine_t *cmdLine);
u08 doExit(cmdLine_t *cmdLine);

// Program counter control block handler prototypes
u08 doIf(cmdLine_t **cmdProgCounter);
u08 doIfElse(cmdLine_t **cmdProgCounter);
u08 doIfElseIf(cmdLine_t **cmdProgCounter);
u08 doIfEnd(cmdLine_t **cmdProgCounter);
u08 doRepeatFor(cmdLine_t **cmdProgCounter);
u08 doRepeatNext(cmdLine_t **cmdProgCounter);
#endif
