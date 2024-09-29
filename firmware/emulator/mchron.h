//*****************************************************************************
// Filename : 'mchron.h'
// Title    : Definitions for mchron and its command handler routines
//*****************************************************************************

#ifndef MCHRON_H
#define MCHRON_H

#include "../avrlibtypes.h"
#include "interpreter.h"

// Number of data buffers to store graphics data in
#define GRAPHICS_BUFFERS	10

// Standard command handler function prototypes
u08 doBeep(cmdLine_t *cmdLine);
u08 doClockFeed(cmdLine_t *cmdLine);
u08 doClockPrint(cmdLine_t *cmdLine);
u08 doClockSelect(cmdLine_t *cmdLine);
u08 doComments(cmdLine_t *cmdLine);
u08 doDbBpAdd(cmdLine_t *cmdLine);
u08 doDbBpCond(cmdLine_t *cmdLine);
u08 doDbBpDelete(cmdLine_t *cmdLine);
u08 doDbBpPrint(cmdLine_t *cmdLine);
u08 doDbContinue(cmdLine_t *cmdLine);
u08 doDbPcSet(cmdLine_t *cmdLine);
u08 doDbSet(cmdLine_t *cmdLine);
u08 doDbStepIn(cmdLine_t *cmdLine);
u08 doDbStepNext(cmdLine_t *cmdLine);
u08 doDbStepOut(cmdLine_t *cmdLine);
u08 doEepromPrint(cmdLine_t *cmdLine);
u08 doEepromReset(cmdLine_t *cmdLine);
u08 doEepromWrite(cmdLine_t *cmdLine);
u08 doExecFile(cmdLine_t *cmdLine);
u08 doExecListPrint(cmdLine_t *cmdLine);
u08 doExecResume(cmdLine_t *cmdLine);
u08 doExecStackPrint(cmdLine_t *cmdLine);
u08 doExit(cmdLine_t *cmdLine);
u08 doGrCopy(cmdLine_t *cmdLine);
u08 doGrInfo(cmdLine_t *cmdLine);
u08 doGrLoadCtrImg(cmdLine_t *cmdLine);
u08 doGrLoadFile(cmdLine_t *cmdLine);
u08 doGrLoadFileImg(cmdLine_t *cmdLine);
u08 doGrLoadFileSpr(cmdLine_t *cmdLine);
u08 doGrReset(cmdLine_t *cmdLine);
u08 doGrSaveFile(cmdLine_t *cmdLine);
u08 doHelp(cmdLine_t *cmdLine);
u08 doHelpCmd(cmdLine_t *cmdLine);
u08 doHelpExpr(cmdLine_t *cmdLine);
u08 doHelpMsg(cmdLine_t *cmdLine);
u08 doLcdActCtrlSet(cmdLine_t *cmdLine);
u08 doLcdBacklightSet(cmdLine_t *cmdLine);
u08 doLcdCursorReset(cmdLine_t *cmdLine);
u08 doLcdDisplaySet(cmdLine_t *cmdLine);
u08 doLcdErase(cmdLine_t *cmdLine);
u08 doLcdGlutEdit(cmdLine_t *cmdLine);
u08 doLcdGlutGrSet(cmdLine_t *cmdLine);
u08 doLcdGlutSizeSet(cmdLine_t *cmdLine);
u08 doLcdGlutHlReset(cmdLine_t *cmdLine);
u08 doLcdGlutHlSet(cmdLine_t *cmdLine);
u08 doLcdInverse(cmdLine_t *cmdLine);
u08 doLcdNcurGrSet(cmdLine_t *cmdLine);
u08 doLcdPrint(cmdLine_t *cmdLine);
u08 doLcdRead(cmdLine_t *cmdLine);
u08 doLcdStartLineSet(cmdLine_t *cmdLine);
u08 doLcdXCursorSet(cmdLine_t *cmdLine);
u08 doLcdYCursorSet(cmdLine_t *cmdLine);
u08 doLcdWrite(cmdLine_t *cmdLine);
u08 doMonochron(cmdLine_t *cmdLine);
u08 doMonoConfig(cmdLine_t *cmdLine);
u08 doPaintAscii(cmdLine_t *cmdLine);
u08 doPaintBuffer(cmdLine_t *cmdLine);
u08 doPaintBufferImg(cmdLine_t *cmdLine);
u08 doPaintBufferSpr(cmdLine_t *cmdLine);
u08 doPaintCircle(cmdLine_t *cmdLine);
u08 doPaintCircleFill(cmdLine_t *cmdLine);
u08 doPaintDot(cmdLine_t *cmdLine);
u08 doPaintSetBg(cmdLine_t *cmdLine);
u08 doPaintSetColor(cmdLine_t *cmdLine);
u08 doPaintSetFg(cmdLine_t *cmdLine);
u08 doPaintLine(cmdLine_t *cmdLine);
u08 doPaintNumber(cmdLine_t *cmdLine);
u08 doPaintRect(cmdLine_t *cmdLine);
u08 doPaintRectFill(cmdLine_t *cmdLine);
u08 doStatsPrint(cmdLine_t *cmdLine);
u08 doStatsReset(cmdLine_t *cmdLine);
u08 doStatsStack(cmdLine_t *cmdLine);
u08 doTimeAlarmPos(cmdLine_t *cmdLine);
u08 doTimeAlarmSet(cmdLine_t *cmdLine);
u08 doTimeAlarmToggle(cmdLine_t *cmdLine);
u08 doTimeDateReset(cmdLine_t *cmdLine);
u08 doTimeDateSet(cmdLine_t *cmdLine);
u08 doTimeFlush(cmdLine_t *cmdLine);
u08 doTimeGet(cmdLine_t *cmdLine);
u08 doTimePrint(cmdLine_t *cmdLine);
u08 doTimeReset(cmdLine_t *cmdLine);
u08 doTimeSet(cmdLine_t *cmdLine);
u08 doVarPrint(cmdLine_t *cmdLine);
u08 doVarReset(cmdLine_t *cmdLine);
u08 doVarSet(cmdLine_t *cmdLine);
u08 doWait(cmdLine_t *cmdLine);
u08 doWaitTimerExpiry(cmdLine_t *cmdLine);
u08 doWaitTimerStart(cmdLine_t *cmdLine);

// Program counter control block handler prototypes
u08 doExecReturn(cmdLine_t **cmdProgCounter);
u08 doIf(cmdLine_t **cmdProgCounter);
u08 doIfElse(cmdLine_t **cmdProgCounter);
u08 doIfElseIf(cmdLine_t **cmdProgCounter);
u08 doIfEnd(cmdLine_t **cmdProgCounter);
u08 doRepeatBreak(cmdLine_t **cmdProgCounter);
u08 doRepeatCont(cmdLine_t **cmdProgCounter);
u08 doRepeatFor(cmdLine_t **cmdProgCounter);
u08 doRepeatNext(cmdLine_t **cmdProgCounter);
#endif
