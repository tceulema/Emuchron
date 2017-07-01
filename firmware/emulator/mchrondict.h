//*****************************************************************************
// Filename : 'mchrondict.h'
// Title    : Command dictionary for mchron interpreter
//*****************************************************************************

#ifndef MCHRONDICT_H
#define MCHRONDICT_H

#include "../ks0108conf.h"
#include "../glcd.h"
#include "interpreter.h"
#include "mchron.h"

// This macro makes an interface to building a command argument
// dictionary to prevent mistakes in the command arg count parameter
#define CMDARGS(a) \
  a, sizeof(a)/sizeof(cmdArg_t)

// This macro returns a handler function in a string
#define HANDLERNAME(a) #a
// This macro returns a regular command handler function and its name
#define CMDHANDLER(a) \
  a, NULL, HANDLERNAME(a)
// This macro returns a control block handler function and its name
#define PCCTRLHANDLER(a) \
  NULL, a, HANDLERNAME(a)

// This macro makes an interface to building a command group
// dictionary to prevent mistakes in the command count parameter
#define CMDGROUP(a) \
  a, sizeof(a)/sizeof(cmdCommand_t)

//
// Building the mchron command dictionary is done in four steps where
// each step is built on top of its preceding step.
// - Step 1: Argument domain value profiles
// - Step 2: Command argument profiles
// - Step 3: Command group profiles
// - Step 4: The complete mchron command dictionary and its size
//

//
// Dictionary build-up step 1: argument domain value profiles
//

// Switch/bit position: 0..1
cmdArgDomain_t argNumOffOn =
{ DOM_NUM_MAX, "", 0, 1, "0 = off, 1 = on" };

// Beep duration: 1..255
cmdArgDomain_t argNumDuration =
{ DOM_NUM_RANGE, "", 1, 255, "msec" };

// Beep frequency: 150..10000
cmdArgDomain_t argNumFrequency =
{ DOM_NUM_RANGE, "", 150, 10000, "Hz" };

// Circle draw pattern: max 3
cmdArgDomain_t argNumCirclePattern =
{ DOM_NUM_MAX, "", 0, 3, "full, half even, half uneven, third" };

// Clock: >=0 limited by number of clocks in emuMonochron[]
cmdArgDomain_t argNumClock =
{ DOM_NUM_MIN, "", 0, 0, "limited by clocks in emuMonochron[]" };

// Rectangle fill pattern: max 5
cmdArgDomain_t argNumRectFill =
{ DOM_NUM_MAX, "", 0, 5, "full, half, 3rd up, 3rd down, inverse, blank" };

// Circle fill pattern: max 5, excluding 4
cmdArgDomain_t argNumCircleFill =
{ DOM_NUM_MAX, "", 0, 5, "full, half, 3rd up, 3rd down, <unsupported>, blank" };

// Circle radius: max 31
cmdArgDomain_t argNumRadius =
{ DOM_NUM_MAX, "", 0, 31, NULL };

// Date day: max 31
cmdArgDomain_t argNumDay =
{ DOM_NUM_RANGE, "", 1, 31, NULL };

// Date month: max 12
cmdArgDomain_t argNumMonth =
{ DOM_NUM_RANGE, "", 1, 12, NULL };

// Date year: max 99
cmdArgDomain_t argNumYear =
{ DOM_NUM_MAX, "", 0, 99, "year in 20xx" };

// Draw x size: 0..128
cmdArgDomain_t argNumXSize =
{ DOM_NUM_MAX, "", 0, GLCD_XPIXELS, NULL };

// Draw y size: 0..64
cmdArgDomain_t argNumYSize =
{ DOM_NUM_MAX, "", 0, GLCD_YPIXELS, NULL };

// Echo command: 'e'cho, 'i'nherit, 's'ilent
cmdArgDomain_t argCharEcho =
{ DOM_CHAR_LIST, "eis", 0, 0, "e = echo, i = inherit, s = silent" };

// Emulator start mode: 'c'ycle mode or 'n'ormal mode
cmdArgDomain_t argCharMode =
{ DOM_CHAR_LIST, "cn", 0, 0, "c = single cycle, n = normal" };

// Emulator eeprom use: 'k'eep or 'r'eset
cmdArgDomain_t argCharEeprom =
{ DOM_CHAR_LIST, "kr", 0, 0, "k = keep, r = reset" };

// Lcd backlight: max 16
cmdArgDomain_t argNumBacklight =
{ DOM_NUM_MAX, "", 0, 16, "0 = dim .. 16 = bright" };

// Lcd color: 'b'ackground or 'f'oreground
cmdArgDomain_t argCharColor =
{ DOM_CHAR_LIST, "bf", 0, 0, "b = background, f = foreground" };

// Lcd controller id: 0..1
cmdArgDomain_t argNumController =
{ DOM_NUM_MAX, "", 0, GLCD_NUM_CONTROLLERS - 1, NULL };

// Lcd controller lcd data
cmdArgDomain_t argNumLcdData =
{ DOM_NUM_MAX, "", 0, 255, NULL };

// Lcd x position: max 127
cmdArgDomain_t argNumPosX =
{ DOM_NUM_MAX, "", 0, GLCD_XPIXELS - 1, NULL };

// Lcd y position: max 63
cmdArgDomain_t argNumPosY =
{ DOM_NUM_MAX, "", 0, GLCD_YPIXELS - 1, NULL };

// Lcd controller x position: max 63
cmdArgDomain_t argNumCtrlPosX =
{ DOM_NUM_MAX, "", 0, GLCD_CONTROLLER_XPIXELS - 1, NULL };

// Lcd controller y page position: max 7
cmdArgDomain_t argNumCtrlPageY =
{ DOM_NUM_MAX, "", 0, GLCD_CONTROLLER_YPAGES - 1, NULL };

// Lcd controller x position: max 63
cmdArgDomain_t argNumCtrlStartLine =
{ DOM_NUM_MAX, "", 0, GLCD_CONTROLLER_YPIXELS - 1, NULL };

// Rectangle fill align: 0..2
cmdArgDomain_t argNumAlign =
{ DOM_NUM_MAX, "", 0, 2, "0 = top left, 1 = bottom left, 2 = auto" };

// Text font: 5x5-proportional, 5x7-non-proportional
cmdArgDomain_t argWordFont =
{ DOM_WORD_LIST, "5x5p\n5x7n", 0, 0, "5x5 proportional, 5x7 non-proportional" };

// Text orientation: 'b'ottom-up, 'h'orizontal, 't'op-down
cmdArgDomain_t argCharOrient =
{ DOM_CHAR_LIST, "bht", 0, 0, "b = bottom-up, h = horizontal, t = top-down" };

// Text scale x (horizontal): 1..64
cmdArgDomain_t argNumScaleX =
{ DOM_NUM_RANGE, "", 1, 64, NULL };

// Text scale y (vertical): 1..32
cmdArgDomain_t argNumScaleY =
{ DOM_NUM_RANGE, "", 1, 32, NULL };

// Time hour: max 23
cmdArgDomain_t argNumHour =
{ DOM_NUM_MAX, "", 0, 23, NULL };

// Time minute/second: max 59
cmdArgDomain_t argNumMinSec =
{ DOM_NUM_MAX, "", 0, 59, NULL };

// Variable name: [a-zA-Z_]* or '*'
cmdArgDomain_t argVarName1 =
{ DOM_VAR_NAME, "", 0, 0, "word of [a-zA-Z_] characters, '*' = all" };

// Variable name: [a-zA-Z_]*
cmdArgDomain_t argVarName2 =
{ DOM_VAR_NAME, "", 0, 0, "word of [a-zA-Z_] characters" };

// Wait delay: max 1E6
cmdArgDomain_t argNumDelay =
{ DOM_NUM_MAX, "", 0, 1E6, "0 = wait for keypress, >0 = wait (msec)" };

// Wait timer expiry: 1..1E6
cmdArgDomain_t argNumExpiry =
{ DOM_NUM_RANGE, "", 1, 1E6, "msec" };

// Commmand: info
cmdArgDomain_t argInfoCommand =
{ DOM_NULL_INFO, "", 0, 0, "mchron command" };

// Comments: info
cmdArgDomain_t argInfoComments =
{ DOM_NULL_INFO, "", 0, 0, "optional ascii text" };

// File name: info
cmdArgDomain_t argInfoFileName =
{ DOM_NULL_INFO, "", 0, 0, "full path or relative to startup directory mchron" };

// Help command: info
cmdArgDomain_t argInfoHelpCmd =
{ DOM_NULL_INFO, "", 0, 0, "mchron command, '*' = all" };

// Number expression: info
cmdArgDomain_t argInfoNumber =
{ DOM_NULL_INFO, "", 0, 0, "expression" };

// Condition expression: info
cmdArgDomain_t argInfoCondition =
{ DOM_NULL_INFO, "", 0, 0, "expression determining block execution" };

// Text: info
cmdArgDomain_t argInfoText =
{ DOM_NULL_INFO, "", 0, 0, "ascii text" };

// Format: info
cmdArgDomain_t argInfoFormat =
{ DOM_NULL_INFO, "", 0, 0, "'c'-style format string containing '\%f', '\%e' or '\%g'" };

// Init expression: info
cmdArgDomain_t argInfoInit =
{ DOM_NULL_INFO, "", 0, 0, "expression executed once at initialization" };

// Post expression: info
cmdArgDomain_t argInfoPost =
{ DOM_NULL_INFO, "", 0, 0, "expression executed after each loop" };

// Assignment expression: info
cmdArgDomain_t argInfoAssign =
{ DOM_NULL_INFO, "", 0, 0, "<variable>=<expression>" };

//
// Dictionary build-up step 2: command argument profiles
//

// Generic profile for command name argument
// This profile is used as a step 1 prefix scan for all other profiles
cmdArg_t argCmd[] =
{ { ARG_WORD,    "command",     &argInfoCommand } };

// Generic profile for end-of-line argument
// This profile is used by commands that do not require any arguments
cmdArg_t argEnd[] =
{ { ARG_END,     "",            NULL } };

// Command '#'
// Argument profile for comments
cmdArg_t argComments[] =
{ { ARG_STR_OPT, "comments",    &argInfoComments } };

// Command 'a*'
// Argument profile for alarm switch position
cmdArg_t argAlarmPos[] =
{ { ARG_UNUM,    "position",    &argNumOffOn },
  { ARG_END,     "",            NULL } };
// Argument profile for set alarm
cmdArg_t argAlarmSet[] =
{ { ARG_UNUM,    "hour",        &argNumHour },
  { ARG_UNUM,    "min",         &argNumMinSec },
  { ARG_END,     "",            NULL } };

// Command 'b*'
// Argument profile for beep
cmdArg_t argBeep[] =
{ { ARG_UNUM,    "frequency",   &argNumFrequency },
  { ARG_UNUM,    "duration",    &argNumDuration },
  { ARG_END,     "",            NULL } };

// Command 'c*'
// Argument profile for clock feed
cmdArg_t argClockFeed[] =
{ { ARG_CHAR,    "mode",        &argCharMode },
  { ARG_END,     "",            NULL } };
// Argument profile for clock select
cmdArg_t argClockSelect[] =
{ { ARG_UNUM,    "clock",       &argNumClock },
  { ARG_END,     "",            NULL } };

// Command 'd*'
// Argument profile for date set
cmdArg_t argDateSet[] =
{ { ARG_UNUM,    "day",         &argNumDay },
  { ARG_UNUM,    "month",       &argNumMonth },
  { ARG_UNUM,    "year",        &argNumYear },
  { ARG_END,     "",            NULL } };

// Command 'e'
// Argument profile for execute command file
cmdArg_t argExecute[] =
{ { ARG_CHAR,    "echo",        &argCharEcho },
  { ARG_STRING,  "filename",    &argInfoFileName } };

// Command 'h*'
// Argument profile for help command dictionary
cmdArg_t argHelpCmd[] =
{ { ARG_WORD,    "command",     &argInfoHelpCmd },
  { ARG_END,     "",            NULL } };
// Argument profile for help expression
cmdArg_t argHelpExpr[] =
{ { ARG_NUM,     "number",      &argInfoNumber },
  { ARG_END,     "",            NULL } };
// Argument profile for help message
cmdArg_t argHelpMsg[] =
{ { ARG_STRING,  "message",     &argInfoText } };

// Command 'i*'
// Argument profile for if-else-if
cmdArg_t argIfElseIf[] =
{ { ARG_WORD,    "condition",   &argInfoCondition },
  { ARG_END,     "",            NULL } };
// Argument profile for if
cmdArg_t argIf[] =
{ { ARG_WORD,    "condition",   &argInfoCondition },
  { ARG_END,     "",            NULL } };

// Command 'l*'
// Argument profile for lcd backlight set
cmdArg_t argLcdBLSet[] =
{ { ARG_UNUM,    "backlight",   &argNumBacklight },
  { ARG_END,     "",            NULL } };
// Argument profile for controller cursor
cmdArg_t argLcdCursSet[] =
{ { ARG_UNUM,    "controller",  &argNumController },
  { ARG_UNUM,    "x", 		&argNumCtrlPosX },
  { ARG_UNUM,    "yline",	&argNumCtrlPageY },
  { ARG_END,     "",            NULL } };
// Argument profile for controller display
cmdArg_t argLcdDispSet[] =
{ { ARG_UNUM,    "controller-0",&argNumOffOn },
  { ARG_UNUM,    "controller-1",&argNumOffOn },
  { ARG_END,     "",            NULL } };
// Argument profile for ncurses backlight support
cmdArg_t argLcdNcurBL[] =
{ { ARG_UNUM,    "support",     &argNumOffOn },
  { ARG_END,     "",            NULL } };
// Argument profile for controller lcd read
cmdArg_t argLcdRead[] =
{ { ARG_UNUM,    "controller",  &argNumController },
  { ARG_WORD,    "variable",    &argVarName2 },
  { ARG_END,     "",            NULL } };
// Argument profile for controller lcd write
cmdArg_t argLcdWrite[] =
{ { ARG_UNUM,    "controller",  &argNumController },
  { ARG_UNUM,    "data",        &argNumLcdData },
  { ARG_END,     "",            NULL } };
// Argument profile for controller startline
cmdArg_t argLcdSLineSet[] =
{ { ARG_UNUM,    "controller-0",&argNumCtrlStartLine },
  { ARG_UNUM,    "controller-1",&argNumCtrlStartLine },
  { ARG_END,     "",            NULL } };

// Command 'm'
// Argument profile for Monochron emulator
cmdArg_t argMonochron[] =
{ { ARG_CHAR,    "mode",        &argCharMode },
  { ARG_CHAR,    "eeprom",      &argCharEeprom },
  { ARG_END,     "",            NULL } };

// Command 'p*'
// Argument profile for paint ascii
cmdArg_t argPaintAscii[] =
{ { ARG_CHAR,    "color",       &argCharColor },
  { ARG_UNUM,    "x",           &argNumPosX },
  { ARG_UNUM,    "y",           &argNumPosY },
  { ARG_WORD,    "font",        &argWordFont },
  { ARG_CHAR,    "orientation", &argCharOrient },
  { ARG_UNUM,    "xscale",      &argNumScaleX },
  { ARG_UNUM,    "yscale",      &argNumScaleY },
  { ARG_STRING,  "text",        &argInfoText } };
// Argument profile for paint circle
cmdArg_t argPaintCircle[] =
{ { ARG_CHAR,    "color",       &argCharColor },
  { ARG_UNUM,    "x",           &argNumPosX },
  { ARG_UNUM,    "y",           &argNumPosY },
  { ARG_UNUM,    "radius",      &argNumRadius },
  { ARG_UNUM,    "pattern",     &argNumCirclePattern},
  { ARG_END,     "",            NULL } };
// Argument profile for paint circle filled
cmdArg_t argPaintCFill[] =
{ { ARG_CHAR,    "color",       &argCharColor },
  { ARG_UNUM,    "x",           &argNumPosX },
  { ARG_UNUM,    "y",           &argNumPosY },
  { ARG_UNUM,    "radius",      &argNumRadius },
  { ARG_UNUM,    "pattern",     &argNumCircleFill },
  { ARG_END,     "",            NULL } };
// Argument profile for paint dot
cmdArg_t argPaintDot[] =
{ { ARG_CHAR,    "color",       &argCharColor },
  { ARG_UNUM,    "x",           &argNumPosX },
  { ARG_UNUM,    "y",           &argNumPosY },
  { ARG_END,     "",            NULL } };
// Argument profile for paint line
cmdArg_t argPaintLine[] =
{ { ARG_CHAR,    "color",       &argCharColor },
  { ARG_UNUM,    "xstart",      &argNumPosX },
  { ARG_UNUM,    "ystart",      &argNumPosY },
  { ARG_UNUM,    "xend",        &argNumPosX },
  { ARG_UNUM,    "yend",        &argNumPosY },
  { ARG_END,     "",            NULL } };
// Argument profile for paint number
cmdArg_t argPaintNumber[] =
{ { ARG_CHAR,    "color",       &argCharColor },
  { ARG_UNUM,    "x",           &argNumPosX },
  { ARG_UNUM,    "y",           &argNumPosY },
  { ARG_WORD,    "font",        &argWordFont },
  { ARG_CHAR,    "orientation", &argCharOrient },
  { ARG_UNUM,    "xscale",      &argNumScaleX },
  { ARG_UNUM,    "yscale",      &argNumScaleY },
  { ARG_NUM,     "number",      &argInfoNumber },
  { ARG_STRING,  "format",      &argInfoFormat } };
// Argument profile for paint rectangle
cmdArg_t argPaintRect[] =
{ { ARG_CHAR,    "color",       &argCharColor },
  { ARG_UNUM,    "x",           &argNumPosX },
  { ARG_UNUM,    "y",           &argNumPosY },
  { ARG_UNUM,    "xsize",       &argNumXSize },
  { ARG_UNUM,    "ysize",       &argNumYSize },
  { ARG_END,     "",            NULL } };
// Argument profile for paint rectangle filled
cmdArg_t argPaintRFill[] =
{ { ARG_CHAR,    "color",       &argCharColor },
  { ARG_UNUM,    "x",           &argNumPosX },
  { ARG_UNUM,    "y",           &argNumPosY },
  { ARG_UNUM,    "xsize",       &argNumXSize },
  { ARG_UNUM,    "ysize",       &argNumYSize },
  { ARG_UNUM,    "align",       &argNumAlign },
  { ARG_UNUM,    "pattern",     &argNumRectFill },
  { ARG_END,     "",            NULL } };

// Command 'r*'
// Argument profile for repeat for
cmdArg_t argRepeatFor[] =
{ { ARG_WORD,    "init",        &argInfoInit },
  { ARG_WORD,    "condition",   &argInfoCondition },
  { ARG_WORD,    "post",        &argInfoPost },
  { ARG_END,     "",            NULL } };

// Command 's*'
// No additional profiles are needed

// Command 't*'
// Argument profile for time set
cmdArg_t argTimeSet[] =
{ { ARG_UNUM,    "hour",        &argNumHour },
  { ARG_UNUM,    "min",         &argNumMinSec },
  { ARG_UNUM,    "sec",         &argNumMinSec },
  { ARG_END,     "",            NULL } };

// Command 'v*'
// Argument profile for variable print
cmdArg_t argVarPrint[] =
{ { ARG_WORD,    "variable",    &argVarName1 },
  { ARG_END,     "",            NULL } };
// Argument profile for variable reset
cmdArg_t argVarReset[] =
{ { ARG_WORD,    "variable",    &argVarName1 },
  { ARG_END,     "",            NULL } };
// Argument profile for variable value set
cmdArg_t argVarSet[] =
{ { ARG_ASSIGN,  "assignment",  &argInfoAssign },
  { ARG_END,     "",            NULL } };

// Command 'w'
// Argument profile for wait
cmdArg_t argWait[] =
{ { ARG_UNUM,    "delay",       &argNumDelay },
  { ARG_END,     "",            NULL } };
// Argument profile for timer expiry
cmdArg_t argExpiry[] =
{ { ARG_UNUM,    "expiry",      &argNumExpiry },
  { ARG_END,     "",            NULL } };

// Command 'x'
// No additional profiles are needed

//
// Dictionary build-up step 3: command group profiles
//
// NOTE: Commands in a command group profile MUST be in alphabetical
// order in order to optimize the linear search method
//

// All commands for command group '#' (comments)
cmdCommand_t cmdGroupComments[] =
{ { "#",   PC_CONTINUE,     CMDARGS(argComments),    CMDHANDLER(doComments),       "comments" } };

// All commands for command group 'a' (alarm)
cmdCommand_t cmdGroupAlarm[] =
{ { "ap",  PC_CONTINUE,     CMDARGS(argAlarmPos),    CMDHANDLER(doAlarmPos),       "set alarm switch position" },
  { "as",  PC_CONTINUE,     CMDARGS(argAlarmSet),    CMDHANDLER(doAlarmSet),       "set alarm time" },
  { "at",  PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doAlarmToggle),    "toggle alarm switch position" } };

// All commands for command group 'b' (beep)
cmdCommand_t cmdGroupBeep[] =
{ { "b",   PC_CONTINUE,     CMDARGS(argBeep),        CMDHANDLER(doBeep),           "play beep" } };

// All commands for command group 'c' (clock)
cmdCommand_t cmdGroupClock[] =
{ { "cf",  PC_CONTINUE,     CMDARGS(argClockFeed),   CMDHANDLER(doClockFeed),      "feed clock time/keyboard events" },
  { "cs",  PC_CONTINUE,     CMDARGS(argClockSelect), CMDHANDLER(doClockSelect),    "select clock" } };

// All commands for command group 'd' (date)
cmdCommand_t cmdGroupDate[] =
{ { "dr",  PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doDateReset),      "reset date to system date" },
  { "ds",  PC_CONTINUE,     CMDARGS(argDateSet),     CMDHANDLER(doDateSet),        "set date" } };

// All commands for command group 'e' (execute)
cmdCommand_t cmdGroupExecute[] =
{ { "e",   PC_CONTINUE,     CMDARGS(argExecute),     CMDHANDLER(doExecute),        "execute commands from file" } };

// All commands for command group 'h' (help)
cmdCommand_t cmdGroupHelp[] =
{ { "h",   PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doHelp),           "show help" },
  { "hc",  PC_CONTINUE,     CMDARGS(argHelpCmd),     CMDHANDLER(doHelpCmd),        "show command details" },
  { "he",  PC_CONTINUE,     CMDARGS(argHelpExpr),    CMDHANDLER(doHelpExpr),       "show expression result" },
  { "hm",  PC_CONTINUE,     CMDARGS(argHelpMsg),     CMDHANDLER(doHelpMsg),        "show help message" } };

// All commands for command group 'i' (if)
cmdCommand_t cmdGroupIf[] =
{ { "iei", PC_IF_ELSE_IF,   CMDARGS(argIfElseIf),    PCCTRLHANDLER(doIfElseIf),    "if else if" },
  { "iel", PC_IF_ELSE,      CMDARGS(argEnd),         PCCTRLHANDLER(doIfElse),      "if else" },
  { "ien", PC_IF_END,       CMDARGS(argEnd),         PCCTRLHANDLER(doIfEnd),       "if end" },
  { "iif", PC_IF,           CMDARGS(argIf),          PCCTRLHANDLER(doIf),          "if" } };

// All commands for command group 'l' (lcd)
cmdCommand_t cmdGroupLcd[] =
{ { "lbs", PC_CONTINUE,     CMDARGS(argLcdBLSet),    CMDHANDLER(doLcdBacklightSet),"set lcd backlight brightness" },
  { "lcr", PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doLcdCursorReset), "reset controller lcd cursors" },
  { "lcs", PC_CONTINUE,     CMDARGS(argLcdCursSet),  CMDHANDLER(doLcdCursorSet),   "set controller lcd cursor" },
  { "lds", PC_CONTINUE,     CMDARGS(argLcdDispSet),  CMDHANDLER(doLcdDisplaySet),  "switch controller lcd display on/off" },
  { "le",  PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doLcdErase),       "erase lcd display" },
  { "li",  PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doLcdInverse),     "inverse lcd display" },
  { "lnb", PC_CONTINUE,     CMDARGS(argLcdNcurBL),   CMDHANDLER(doLcdNcurBLSet),   "set ncurses backlight support" },
  { "lp",  PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doLcdPrint),       "print controller state/registers" },
  { "lr",  PC_CONTINUE,     CMDARGS(argLcdRead),     CMDHANDLER(doLcdRead),        "read controller lcd data in variable" },
  { "lss", PC_CONTINUE,     CMDARGS(argLcdSLineSet), CMDHANDLER(doLcdStartLineSet),"set controller lcd start line" },
  { "lw",  PC_CONTINUE,     CMDARGS(argLcdWrite),    CMDHANDLER(doLcdWrite),       "write data to controller lcd" } };

// All commands for command group 'm' (monochron)
cmdCommand_t cmdGroupMonochron[] =
{ { "m",   PC_CONTINUE,     CMDARGS(argMonochron),   CMDHANDLER(doMonochron),      "run monochron application" } };

// All commands for command group 'p' (paint)
cmdCommand_t cmdGroupPaint[] =
{ { "pa",  PC_CONTINUE,     CMDARGS(argPaintAscii),  CMDHANDLER(doPaintAscii),     "paint ascii" },
  { "pc",  PC_CONTINUE,     CMDARGS(argPaintCircle), CMDHANDLER(doPaintCircle),    "paint circle" },
  { "pcf", PC_CONTINUE,     CMDARGS(argPaintCFill),  CMDHANDLER(doPaintCircleFill),"paint filled circle" },
  { "pd",  PC_CONTINUE,     CMDARGS(argPaintDot),    CMDHANDLER(doPaintDot),       "paint dot" },
  { "pl",  PC_CONTINUE,     CMDARGS(argPaintLine),   CMDHANDLER(doPaintLine),      "paint line" },
  { "pn",  PC_CONTINUE,     CMDARGS(argPaintNumber), CMDHANDLER(doPaintNumber),    "paint number" },
  { "pr",  PC_CONTINUE,     CMDARGS(argPaintRect),   CMDHANDLER(doPaintRect),      "paint rectangle" },
  { "prf", PC_CONTINUE,     CMDARGS(argPaintRFill),  CMDHANDLER(doPaintRectFill),  "paint filled rectangle" } };

// All commands for command group 'r' (repeat)
cmdCommand_t cmdGroupRepeat[] =
{ { "rf",  PC_REPEAT_FOR,   CMDARGS(argRepeatFor),   PCCTRLHANDLER(doRepeatFor),   "repeat for" },
  { "rn",  PC_REPEAT_NEXT,  CMDARGS(argEnd),         PCCTRLHANDLER(doRepeatNext),  "repeat next" } };

// All commands for command group 's' (statistics)
cmdCommand_t cmdGroupStats[] =
{ { "sp",  PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doStatsPrint),     "print application statistics" },
  { "sr",  PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doStatsReset),     "reset application statistics" } };

// All commands for command group 't' (time)
cmdCommand_t cmdGroupTime[] =
{ { "tf",  PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doTimeFlush),      "flush time/date to clock" },
  { "tp",  PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doTimePrint),      "print time/date/alarm" },
  { "tr",  PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doTimeReset),      "reset time to system time" },
  { "ts",  PC_CONTINUE,     CMDARGS(argTimeSet),     CMDHANDLER(doTimeSet),        "set time" } };

// All commands for command group 'v' (variable)
cmdCommand_t cmdGroupVar[] =
{ { "vp",  PC_CONTINUE,     CMDARGS(argVarPrint),    CMDHANDLER(doVarPrint),       "print value variable(s)" },
  { "vr",  PC_CONTINUE,     CMDARGS(argVarReset),    CMDHANDLER(doVarReset),       "reset value variable(s)" },
  { "vs",  PC_CONTINUE,     CMDARGS(argVarSet),      CMDHANDLER(doVarSet),         "set value variable" } };

// All commands for command group 'w' (wait)
cmdCommand_t cmdGroupWait[] =
{ { "w",   PC_CONTINUE,     CMDARGS(argWait),        CMDHANDLER(doWait),           "wait for keypress or time" },
  { "wte", PC_CONTINUE,     CMDARGS(argExpiry),      CMDHANDLER(doWaitTimerExpiry),"wait for timer expiry" },
  { "wts", PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doWaitTimerStart), "start expiry timer" } };

// All commands for command group 'x' (exit)
cmdCommand_t cmdGroupExit[] =
{ { "x",   PC_CONTINUE,     CMDARGS(argEnd),         CMDHANDLER(doExit),           "exit mchron" } };

//
// Dictionary build-up step 4: the complete mchron command dictionary and its size
//
cmdDict_t cmdDictMchron[] =
{
  { '#', "comments",   CMDGROUP(cmdGroupComments) },
  { 'a', "alarm",      CMDGROUP(cmdGroupAlarm) },
  { 'b', "beep",       CMDGROUP(cmdGroupBeep) },
  { 'c', "clock",      CMDGROUP(cmdGroupClock) },
  { 'd', "date",       CMDGROUP(cmdGroupDate) },
  { 'e', "execute",    CMDGROUP(cmdGroupExecute) },
  { 'f', "-",          CMDGROUP(NULL) },
  { 'g', "-",          CMDGROUP(NULL) },
  { 'h', "help",       CMDGROUP(cmdGroupHelp) },
  { 'i', "if",         CMDGROUP(cmdGroupIf) },
  { 'j', "-",          CMDGROUP(NULL) },
  { 'k', "-",          CMDGROUP(NULL) },
  { 'l', "lcd",        CMDGROUP(cmdGroupLcd) },
  { 'm', "monochron",  CMDGROUP(cmdGroupMonochron) },
  { 'n', "-",          CMDGROUP(NULL) },
  { 'o', "-",          CMDGROUP(NULL) },
  { 'p', "paint",      CMDGROUP(cmdGroupPaint) },
  { 'q', "-",          CMDGROUP(NULL) },
  { 'r', "repeat",     CMDGROUP(cmdGroupRepeat) },
  { 's', "statistics", CMDGROUP(cmdGroupStats) },
  { 't', "time",       CMDGROUP(cmdGroupTime) },
  { 'u', "-",          CMDGROUP(NULL) },
  { 'v', "variable",   CMDGROUP(cmdGroupVar) },
  { 'w', "wait",       CMDGROUP(cmdGroupWait) },
  { 'x', "exit",       CMDGROUP(cmdGroupExit) },
  { 'y', "-",          CMDGROUP(NULL) },
  { 'z', "-",          CMDGROUP(NULL) }
};
int cmdDictCount = sizeof(cmdDictMchron) / sizeof(cmdDict_t);
#endif
