//*****************************************************************************
// Filename : 'mchrondict.h'
// Title    : Command dictionary for mchron interpreter
//*****************************************************************************

#ifndef MCHRONDICT_H
#define MCHRONDICT_H

#include "../ks0108conf.h"
#include "mchron.h"

// This macro returns a dictionary domain profile and its name
#define DOMAIN(a, b, c, d, e, f) \
  cmdDomain_t a = {#a, #b, b, c, d, e, f}

// This macro makes an interface to building a command argument dictionary to
// prevent mistakes in the command arg count parameter as well as its name
#define CMDARGS(a) \
  #a, a, sizeof(a)/sizeof(cmdArg_t)

// This macro makes an interface to building a command group dictionary to
// prevent mistakes in the command count parameter
#define CMDGROUP(a) \
  a, sizeof(a)/sizeof(cmdCommand_t)

// This macro returns a command argument type and its name
#define ARGTYPE(a) \
  #a, a

// This macro returns a program counter control block (pcb) type and its name
#define PCBTYPE(a) \
  #a, a

// This macro returns a regular command handler function and its name
#define CMDHANDLER(a) \
  #a, a, NULL

// This macro returns a control block handler function and its name
#define PCCTRLHANDLER(a) \
  #a, NULL, a

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
// The macro DOMAIN() below generates the following static data:
// cmdDomain_t domNumOffOn =
//   {"domNumOffOn", DOM_NUM_RANGE, "DOM_NUM_RANGE", NULL, 0, 1,
//    "0 = off, 1 = on"};
DOMAIN(domNumOffOn, \
  DOM_NUM_RANGE, NULL, 0, 1, "0 = off, 1 = on");

// Beep duration: 1..255
DOMAIN(domNumDuration, \
  DOM_NUM_RANGE, NULL, 1, 255, "msec");

// Beep frequency: 150..10000
DOMAIN(domNumFrequency, \
  DOM_NUM_RANGE, NULL, 150, 10000, "Hz");

// Byte data: 0..255
DOMAIN(domNumByteData, \
  DOM_NUM_RANGE, NULL, 0, 255, NULL);

// Eeprom address: 0..1024
DOMAIN(domNumKBAddress, \
  DOM_NUM_RANGE, NULL, 0, 1023, NULL);

// Circle draw pattern: 0..3
DOMAIN(domNumCirclePattern, \
  DOM_NUM_RANGE, NULL, 0, 3, "full, half even, half uneven, third");

// Clock: >=0, manually limited by number of clocks in emuMonochron[]
DOMAIN(domNumClock, \
  DOM_NUM_RANGE, NULL, 0, 28, "0 = detach from clock, other = select clock");

// Fill pattern: 0..5
DOMAIN(domNumFillPattern, \
  DOM_NUM_RANGE, NULL, 0, 5, "full, half, 3rd up, 3rd down, inverse, blank");

// Circle radius: 0..31
DOMAIN(domNumRadius, \
  DOM_NUM_RANGE, NULL, 0, 31, NULL);

// Date day: 1..31
DOMAIN(domNumDay, \
  DOM_NUM_RANGE, NULL, 1, 31, NULL);

// Date month: 1..12
DOMAIN(domNumMonth, \
  DOM_NUM_RANGE, NULL, 1, 12, NULL);

// Date year: 0..99
DOMAIN(domNumYear, \
  DOM_NUM_RANGE, NULL, 0, 99, "year in 20xx");

// Draw x size: 0..128
DOMAIN(domNumXSize, \
  DOM_NUM_RANGE, NULL, 0, GLCD_XPIXELS, NULL);

// Draw y size: 0..64
DOMAIN(domNumYSize, \
  DOM_NUM_RANGE, NULL, 0, GLCD_YPIXELS, NULL);

// Echo command: 'e'cho, 'i'nherit, 's'ilent
DOMAIN(domCharEcho, \
  DOM_CHAR_VAL, "eis", 0, 0, "e = echo, i = inherit, s = silent");

// Emulator start mode: 'c'ycle mode or 'r'un mode
DOMAIN(domCharMode, \
  DOM_CHAR_VAL, "cr", 0, 0, "c = single cycle, r = run");

// Lcd backlight: 0..16
DOMAIN(domNumBacklight, \
  DOM_NUM_RANGE, NULL, 0, 16, "0 = dim .. 16 = bright");

// Lcd color: 0..1: 0 = GLCD_OFF, 1 = GLCD_ON
DOMAIN(domNumColor, \
  DOM_NUM_RANGE, NULL, 0, 1, "0 = off (black), 1 = on (white)");

// Lcd controller id: 0..1
DOMAIN(domNumController, \
  DOM_NUM_RANGE, NULL, 0, GLCD_NUM_CONTROLLERS - 1, NULL);

// Lcd x position: 0..127
DOMAIN(domNumPosX, \
  DOM_NUM_RANGE, NULL, 0, GLCD_XPIXELS - 1, NULL);

// Lcd y position: 0..63
DOMAIN(domNumPosY, \
  DOM_NUM_RANGE, NULL, 0, GLCD_YPIXELS - 1, NULL);

// Lcd controller x position: 0..63
DOMAIN(domNumCtrlPosX, \
  DOM_NUM_RANGE, NULL, 0, GLCD_CONTROLLER_XPIXELS - 1, NULL);

// Lcd controller y page position: 0..7
DOMAIN(domNumCtrlPageY, \
  DOM_NUM_RANGE, NULL, 0, GLCD_CONTROLLER_YPAGES - 1, NULL);

// Lcd controller x position: 0..63
DOMAIN(domNumCtrlStartLine, \
  DOM_NUM_RANGE, NULL, 0, GLCD_CONTROLLER_YPIXELS - 1, NULL);

// Lcd glut window resize axis: w, h
DOMAIN(domCharAxisResize, \
  DOM_CHAR_VAL, "wh", 0, 0, "w = width, h = height");

// Lcd glut window axis size: x: 132..2080, y: 70..1056
DOMAIN(domNumAxisSize, \
  DOM_NUM_RANGE, NULL, 66, 2080, "width-range = 130..2080, height-range = 66..1056");

// Rectangle fill align: 0..2
DOMAIN(domNumAlign, \
  DOM_NUM_RANGE, NULL, 0, 2, "0 = top left, 1 = bottom left, 2 = auto");

// Graphics buffer: 0..9
DOMAIN(domNumBufferId, \
  DOM_NUM_RANGE, NULL, 0, GRAPHICS_BUFFERS - 1, NULL);

// Graphics buffer or all: -1..9
DOMAIN(domNumBufferAllId, \
  DOM_NUM_RANGE, NULL, -1, GRAPHICS_BUFFERS - 1, "-1 = all, other = buffer");

// Sprite width: 1..128
DOMAIN(domNumFrameX, \
  DOM_NUM_RANGE, NULL, 1, GLCD_XPIXELS, "sprite width");

// Sprite height: 1..32
DOMAIN(domNumFrameY, \
  DOM_NUM_RANGE, NULL, 1, 32, "sprite height");

// Image width: 1..128
DOMAIN(domNumImageX, \
  DOM_NUM_RANGE, NULL, 1, GLCD_XPIXELS, "image width");

// Image height: 1..64
DOMAIN(domNumImageY, \
  DOM_NUM_RANGE, NULL, 1, GLCD_YPIXELS, "image height");

// Data element x offset: 0..1023
DOMAIN(domNumElmXOffset, \
  DOM_NUM_RANGE, NULL, 0, GLCD_XPIXELS * GLCD_YPIXELS / 8 - 1, "data element x offset");

// Data element y offset: 0..31
DOMAIN(domNumElmYOffset, \
  DOM_NUM_RANGE, NULL, 0, 31, "data element y offset");

// Image data x size: 0..128
DOMAIN(domNumImageXSize, \
  DOM_NUM_RANGE, NULL, 0, GLCD_XPIXELS, NULL);

// Image data y size: 0..32
DOMAIN(domNumImageYSize, \
  DOM_NUM_RANGE, NULL, 0, 32, NULL);

// Sprite frame: 0..127
DOMAIN(domNumFrame, \
  DOM_NUM_RANGE, NULL, 0, 127, NULL);

// Graphics data format: 'b'yte (8-bit), 'w'ord (16-bit), 'd'ouble word (32-bit)
DOMAIN(domCharDataFormat, \
  DOM_CHAR_VAL, "bwd", 0, 0, "b = 8-bit, w = 16-bit, d = 32-bit");

// Data elements per output line: 0..128
DOMAIN(domNumElements, \
  DOM_NUM_RANGE, NULL, 0, 128, "0 = max 80 chars/line, other = elements/line");

// Text font: '5x5-proportional, 5x7-monospace
DOMAIN(domWordFont, \
  DOM_WORD_VAL, "5x5p\n5x7m", 0, 0, "5x5p = 5x5 proportional, 5x7m = 5x7 monospace");

// Text orientation: 'b'ottom-up, 'h'orizontal, 't'op-down
DOMAIN(domCharOrient, \
  DOM_CHAR_VAL, "bht", 0, 0, "b = bottom-up, h = horizontal, t = top-down");

// Text scale x (horizontal): 1..64
DOMAIN(domNumScaleX, \
  DOM_NUM_RANGE, NULL, 1, 64, NULL);

// Text scale y (vertical): 1..32
DOMAIN(domNumScaleY, \
  DOM_NUM_RANGE, NULL, 1, 32, NULL);

// Time hour: 0..23
DOMAIN(domNumHour, \
  DOM_NUM_RANGE, NULL, 0, 23, NULL);

// Time minute/second: 0..59
DOMAIN(domNumMinSec, \
  DOM_NUM_RANGE, NULL, 0, 59, NULL);

// Variable name: [a-zA-Z_]+ where value 'null' means to ignore it
DOMAIN(domStrVarName, \
  DOM_WORD_REGEX, "^[a-zA-Z_]+$", 0, 0, "word of [a-zA-Z_] characters, 'null' = ignore");

// Variable name: [a-zA-Z_]+ or '.'
DOMAIN(domStrVarNameAll, \
  DOM_WORD_REGEX, "^(\\.|[a-zA-Z_]+)$", 0, 0, "word of [a-zA-Z_] characters, '.' = all");

// Variable name: regex pattern
DOMAIN(domStrVarPattern, \
  DOM_STRING, NULL, 0, 0, "variable name regex pattern, '.' = all");

// Wait delay: 0..1E6
DOMAIN(domNumDelay, \
  DOM_NUM_RANGE, NULL, 0, 1E6, "0 = wait for keypress, other = wait (msec)");

// Wait timer expiry: 1..1E6
DOMAIN(domNumExpiry, \
  DOM_NUM_RANGE, NULL, 1, 1E6, "msec");

// Comments: info
DOMAIN(domStrComments, \
  DOM_STRING_OPT, "", 0, 0, "optional ascii text");

// File name: info
DOMAIN(domStrFileName, \
  DOM_STRING, NULL, 0, 0, "full path or relative to startup directory mchron");

// Help: command search property: 'a'rgument, 'd'escription, 'n'ame, '.' = all
DOMAIN(domCharSearch, \
  DOM_CHAR_VAL, "adn.", 0, 0, "a = argument, d = descr, n = name, . = all");

// Help: command search regex pattern
DOMAIN(domStrSearchPattern, \
  DOM_STRING, NULL, 0, 0, "mchron command search regex pattern, '.' = all");

// Number expression: info
DOMAIN(domNumExpr, \
  DOM_NUM, NULL, 0, 0, "expression");

// Block condition expression: info
DOMAIN(domNumConditionBlock, \
  DOM_NUM, NULL, 0, 0, "expression determining block execution");

// Stack level: 0..(CMD_STACK_DEPTH_MAX - 1)
DOMAIN(domNumStackLevel, \
  DOM_NUM_RANGE, NULL, 0, 7, "script execution stack level");

// Script line number: 1..10000
DOMAIN(domNumLineNumber, \
  DOM_NUM_RANGE, NULL, 1, 1E4, "script line number");

// Script program counter line number: 0..10000
DOMAIN(domNumPcLineNumber, \
  DOM_NUM_RANGE, NULL, 0, 1E4, "script line number, 0 = <eof>");

// Script line number range: -1..10000
DOMAIN(domNumLineRange, \
  DOM_NUM_RANGE, NULL, -1, 1E4, "script line number range, -1 = full range");

// Debug breakpoint number or all: 1..1000, 0 is all
DOMAIN(domNumBreakpoint, \
  DOM_NUM_RANGE, NULL, 0, 1E3, "breakpoint number, 0 = all");

// Debug breakpoint condition expression: Info
DOMAIN(domStrConditionBp, \
  DOM_STRING, NULL, 0, 0, "expression determining execution halt");

// Breakpoint condition expression: info
DOMAIN(domNumConditionBp, \
  DOM_NUM, NULL, 0, 0, "expression determining execution halt");

// Text: info
DOMAIN(domStrText, \
  DOM_STRING, NULL, 0, 0, "ascii text");

// Format: info
DOMAIN(domStrFormat, \
  DOM_STRING, NULL, 0, 0, "'c'-style format string containing '\%f', '\%e' or '\%g'");

// Init expression: info
DOMAIN(domNumInit, \
  DOM_NUM, NULL, 0, 0, "expression executed once at initialization");

// Post expression: info
DOMAIN(domNumPost, \
  DOM_NUM, NULL, 0, 0, "expression executed after each loop");

// Assignment expression: info
DOMAIN(domNumAssign, \
  DOM_NUM_ASSIGN, NULL, 0, 0, "<variable>=<expression>");

//
// Dictionary build-up step 2: command argument profiles
//
// This is about creating command arguments using the domains above. Commands
// without arguments will not have an entry in here.
//

// Command '#'
// Argument profile for comments
cmdArg_t argComments[] =
{ { ARGTYPE(ARG_STRING), "comments",     &domStrComments } };

// Command 'b*'
// Argument profile for beep
cmdArg_t argBeep[] =
{ { ARGTYPE(ARG_NUM),    "frequency",    &domNumFrequency },
  { ARGTYPE(ARG_NUM),    "duration",     &domNumDuration } };

// Command 'c*'
// Argument profile for clock feed
cmdArg_t argClockFeed[] =
{ { ARGTYPE(ARG_CHAR),   "startmode",    &domCharMode } };
// Argument profile for clock select
cmdArg_t argClockSelect[] =
{ { ARGTYPE(ARG_NUM),    "clock",        &domNumClock } };

// Command 'd*'
// Argument profile for breakpoint add
cmdArg_t argDbBpAdd[] =
{ { ARGTYPE(ARG_NUM),    "stacklevel",   &domNumStackLevel },
  { ARGTYPE(ARG_NUM),    "linenumber",   &domNumLineNumber },
  { ARGTYPE(ARG_STRING), "condition",    &domStrConditionBp } };
// Argument profile for breakpoint delete
cmdArg_t argDbBpDelete[] =
{ { ARGTYPE(ARG_NUM),    "stacklevel",   &domNumStackLevel },
  { ARGTYPE(ARG_NUM),    "breakpoint",   &domNumBreakpoint } };
// Argument profile for conditional debug breakpoint
cmdArg_t argDbBpCond[] =
{ { ARGTYPE(ARG_NUM),    "condition",    &domNumConditionBp } };
// Argument profile for setting script program counter
cmdArg_t argDbPcSet[] =
{ { ARGTYPE(ARG_NUM),    "linenumber",   &domNumPcLineNumber } };
// Argument profile for activating script debugging
cmdArg_t argDbSet[] =
{ { ARGTYPE(ARG_NUM),    "enable",       &domNumOffOn } };

// Command 'e*'
// Argument profile for printing stack level source list range
cmdArg_t argExecListPrint[] =
{ { ARGTYPE(ARG_NUM),    "stacklevel",   &domNumStackLevel },
  { ARGTYPE(ARG_NUM),    "range",        &domNumLineRange } };
// Argument profile for execute command file
cmdArg_t argExecute[] =
{ { ARGTYPE(ARG_CHAR),   "echo",         &domCharEcho },
  { ARGTYPE(ARG_STRING), "filename",     &domStrFileName } };

// Command 'g*'
// Argument profile for copying a graphics buffer
cmdArg_t argGrCopy[] =
{ { ARGTYPE(ARG_NUM),    "from",         &domNumBufferId },
  { ARGTYPE(ARG_NUM),    "to",           &domNumBufferId } };
// Argument profile for showing graphics buffer info
cmdArg_t argGrInfo[] =
{ { ARGTYPE(ARG_NUM),    "buffer",       &domNumBufferAllId } };
// Argument profile for loading graphics lcd controller image data in buffer
cmdArg_t argGrLoadCtrImg[] =
{ { ARGTYPE(ARG_NUM),    "buffer",       &domNumBufferId },
  { ARGTYPE(ARG_CHAR),   "format",       &domCharDataFormat },
  { ARGTYPE(ARG_NUM),    "x",            &domNumPosX },
  { ARGTYPE(ARG_NUM),    "y",            &domNumPosY },
  { ARGTYPE(ARG_NUM),    "xsize",        &domNumXSize },
  { ARGTYPE(ARG_NUM),    "ysize",        &domNumYSize } };
// Argument profile for loading graphics file bitmap data in buffer
cmdArg_t argGrLoadFile[] =
{ { ARGTYPE(ARG_NUM),    "buffer",       &domNumBufferId },
  { ARGTYPE(ARG_CHAR),   "format",       &domCharDataFormat },
  { ARGTYPE(ARG_STRING), "filename",     &domStrFileName } };
// Argument profile for loading graphics file image data in buffer
cmdArg_t argGrLoadFileImg[] =
{ { ARGTYPE(ARG_NUM),    "buffer",       &domNumBufferId },
  { ARGTYPE(ARG_CHAR),   "format",       &domCharDataFormat },
  { ARGTYPE(ARG_NUM),    "xsize",        &domNumImageX },
  { ARGTYPE(ARG_NUM),    "ysize",        &domNumImageY },
  { ARGTYPE(ARG_STRING), "filename",     &domStrFileName } };
// Argument profile for loading graphics file sprite data in buffer
cmdArg_t argGrLoadFileSpr[] =
{ { ARGTYPE(ARG_NUM),    "buffer",       &domNumBufferId },
  { ARGTYPE(ARG_NUM),    "xsize",        &domNumFrameX },
  { ARGTYPE(ARG_NUM),    "ysize",        &domNumFrameY },
  { ARGTYPE(ARG_STRING), "filename",     &domStrFileName } };
// Argument profile for resetting a graphics buffer
cmdArg_t argGrReset[] =
{ { ARGTYPE(ARG_NUM),    "buffer",       &domNumBufferAllId } };
// Argument profile for saving graphics buffer data in file
cmdArg_t argGrSaveFile[] =
{ { ARGTYPE(ARG_NUM),    "buffer",       &domNumBufferId },
  { ARGTYPE(ARG_NUM),    "width",        &domNumElements },
  { ARGTYPE(ARG_STRING), "filename",     &domStrFileName } };

// Command 'h*'
// Argument profile for help command dictionary using command name
cmdArg_t argHelpCmd[] =
{ { ARGTYPE(ARG_CHAR),   "search",       &domCharSearch },
  { ARGTYPE(ARG_STRING), "pattern",      &domStrSearchPattern } };
// Argument profile for help expression
cmdArg_t argHelpExpr[] =
{ { ARGTYPE(ARG_NUM),    "value",        &domNumExpr } };
// Argument profile for help message
cmdArg_t argHelpMsg[] =
{ { ARGTYPE(ARG_STRING), "message",      &domStrComments } };

// Command 'i*'
// Argument profile for if-else-if
cmdArg_t argIfElseIf[] =
{ { ARGTYPE(ARG_NUM),    "condition",    &domNumConditionBlock } };
// Argument profile for if
cmdArg_t argIf[] =
{ { ARGTYPE(ARG_NUM),    "condition",    &domNumConditionBlock } };

// Command 'l*'
// Argument profile for set active lcd controller
cmdArg_t argLcdActCtrlSet[] =
{ { ARGTYPE(ARG_NUM),    "controller",   &domNumController } };
// Argument profile for lcd backlight set
cmdArg_t argLcdBacklightSet[] =
{ { ARGTYPE(ARG_NUM),    "backlight",    &domNumBacklight } };
// Argument profile for controller display
cmdArg_t argLcdDisplaySet[] =
{ { ARGTYPE(ARG_NUM),    "controller-0", &domNumOffOn },
  { ARGTYPE(ARG_NUM),    "controller-1", &domNumOffOn } };
// Argument profile for glut graphic options support
cmdArg_t argLcdGlutGrSet[] =
{ { ARGTYPE(ARG_NUM),    "pixelbezel",   &domNumOffOn },
  { ARGTYPE(ARG_NUM),    "gridlines",    &domNumOffOn } };
// Argument profile for glut glcd pixel highlight
cmdArg_t argLcdGlutHlSet[] =
{ { ARGTYPE(ARG_NUM),    "x",            &domNumPosX },
  { ARGTYPE(ARG_NUM),    "y",            &domNumPosY } };
// Argument profile for set glut window size
cmdArg_t argLcdGlutSizeSet[] =
{ { ARGTYPE(ARG_CHAR),   "axis",         &domCharAxisResize },
  { ARGTYPE(ARG_NUM),    "size",         &domNumAxisSize } };
// Argument profile for ncurses backlight support
cmdArg_t argLcdNcurGrSet[] =
{ { ARGTYPE(ARG_NUM),    "backlight",    &domNumOffOn } };
// Argument profile for controller lcd read
cmdArg_t argLcdRead[] =
{ { ARGTYPE(ARG_STRING), "variable",     &domStrVarName } };
// Argument profile for controller lcd write
cmdArg_t argLcdWrite[] =
{ { ARGTYPE(ARG_NUM),    "data",         &domNumByteData } };
// Argument profile for controller startline
cmdArg_t argLcdStartLineSet[] =
{ { ARGTYPE(ARG_NUM),    "controller-0", &domNumCtrlStartLine },
  { ARGTYPE(ARG_NUM),    "controller-1", &domNumCtrlStartLine } };
// Argument profile for controller x cursor
cmdArg_t argLcdXCursorSet[] =
{ { ARGTYPE(ARG_NUM),    "x",            &domNumCtrlPosX } };
// Argument profile for controller y cursor
cmdArg_t argLcdYCursorSet[] =
{ { ARGTYPE(ARG_NUM),    "yline",        &domNumCtrlPageY } };

// Command 'm*'
// Argument profile for Monochron application emulator
cmdArg_t argMonochron[] =
{ { ARGTYPE(ARG_CHAR),   "startmode",    &domCharMode } };
// Argument profile for Monochron config pages emulator
cmdArg_t argMonoConfig[] =
{ { ARGTYPE(ARG_CHAR),   "startmode",    &domCharMode },
  { ARGTYPE(ARG_NUM),    "timeout",      &domNumOffOn },
  { ARGTYPE(ARG_NUM),    "restart",      &domNumOffOn } };
// Argument profile for Monochron eeprom write
cmdArg_t argEepromWrite[] =
{ { ARGTYPE(ARG_NUM),    "address",      &domNumKBAddress },
  { ARGTYPE(ARG_NUM),    "data",         &domNumByteData } };

// Command 'p*'
// Argument profile for paint ascii
cmdArg_t argPaintAscii[] =
{ { ARGTYPE(ARG_NUM),    "x",            &domNumPosX },
  { ARGTYPE(ARG_NUM),    "y",            &domNumPosY },
  { ARGTYPE(ARG_STRING), "font",         &domWordFont },
  { ARGTYPE(ARG_CHAR),   "orientation",  &domCharOrient },
  { ARGTYPE(ARG_NUM),    "xscale",       &domNumScaleX },
  { ARGTYPE(ARG_NUM),    "yscale",       &domNumScaleY },
  { ARGTYPE(ARG_STRING), "text",         &domStrText } };
// Argument profile for paint buffer
cmdArg_t argPaintBuffer[] =
{ { ARGTYPE(ARG_NUM),    "buffer",       &domNumBufferId },
  { ARGTYPE(ARG_NUM),    "x",            &domNumPosX },
  { ARGTYPE(ARG_NUM),    "y",            &domNumPosY },
  { ARGTYPE(ARG_NUM),    "xo",           &domNumElmXOffset },
  { ARGTYPE(ARG_NUM),    "yo",           &domNumElmYOffset },
  { ARGTYPE(ARG_NUM),    "xsize",        &domNumImageXSize },
  { ARGTYPE(ARG_NUM),    "ysize",        &domNumImageYSize } };
// Argument profile for paint buffer sprite
cmdArg_t argPaintBufferSpr[] =
{ { ARGTYPE(ARG_NUM),    "buffer",       &domNumBufferId },
  { ARGTYPE(ARG_NUM),    "x",            &domNumPosX },
  { ARGTYPE(ARG_NUM),    "y",            &domNumPosY },
  { ARGTYPE(ARG_NUM),    "frame",        &domNumFrame } };
// Argument profile for paint buffer image
cmdArg_t argPaintBufferImg[] =
{ { ARGTYPE(ARG_NUM),    "buffer",       &domNumBufferId },
  { ARGTYPE(ARG_NUM),    "x",            &domNumPosX },
  { ARGTYPE(ARG_NUM),    "y",            &domNumPosY } };
// Argument profile for paint circle
cmdArg_t argPaintCircle[] =
{ { ARGTYPE(ARG_NUM),    "x",            &domNumPosX },
  { ARGTYPE(ARG_NUM),    "y",            &domNumPosY },
  { ARGTYPE(ARG_NUM),    "radius",       &domNumRadius },
  { ARGTYPE(ARG_NUM),    "pattern",      &domNumCirclePattern} };
// Argument profile for paint circle filled
cmdArg_t argPaintCircleFill[] =
{ { ARGTYPE(ARG_NUM),    "x",            &domNumPosX },
  { ARGTYPE(ARG_NUM),    "y",            &domNumPosY },
  { ARGTYPE(ARG_NUM),    "radius",       &domNumRadius },
  { ARGTYPE(ARG_NUM),    "pattern",      &domNumFillPattern } };
// Argument profile for paint dot
cmdArg_t argPaintDot[] =
{ { ARGTYPE(ARG_NUM),    "x",            &domNumPosX },
  { ARGTYPE(ARG_NUM),    "y",            &domNumPosY } };
// Argument profile for paint line
cmdArg_t argPaintLine[] =
{ { ARGTYPE(ARG_NUM),    "xstart",       &domNumPosX },
  { ARGTYPE(ARG_NUM),    "ystart",       &domNumPosY },
  { ARGTYPE(ARG_NUM),    "xend",         &domNumPosX },
  { ARGTYPE(ARG_NUM),    "yend",         &domNumPosY } };
// Argument profile for paint number
cmdArg_t argPaintNumber[] =
{ { ARGTYPE(ARG_NUM),    "x",            &domNumPosX },
  { ARGTYPE(ARG_NUM),    "y",            &domNumPosY },
  { ARGTYPE(ARG_STRING), "font",         &domWordFont },
  { ARGTYPE(ARG_CHAR),   "orientation",  &domCharOrient },
  { ARGTYPE(ARG_NUM),    "xscale",       &domNumScaleX },
  { ARGTYPE(ARG_NUM),    "yscale",       &domNumScaleY },
  { ARGTYPE(ARG_NUM),    "value",        &domNumExpr },
  { ARGTYPE(ARG_STRING), "format",       &domStrFormat } };
// Argument profile for paint rectangle
cmdArg_t argPaintRect[] =
{ { ARGTYPE(ARG_NUM),    "x",            &domNumPosX },
  { ARGTYPE(ARG_NUM),    "y",            &domNumPosY },
  { ARGTYPE(ARG_NUM),    "xsize",        &domNumXSize },
  { ARGTYPE(ARG_NUM),    "ysize",        &domNumYSize } };
// Argument profile for paint rectangle filled
cmdArg_t argPaintRectFill[] =
{ { ARGTYPE(ARG_NUM),    "x",            &domNumPosX },
  { ARGTYPE(ARG_NUM),    "y",            &domNumPosY },
  { ARGTYPE(ARG_NUM),    "xsize",        &domNumXSize },
  { ARGTYPE(ARG_NUM),    "ysize",        &domNumYSize },
  { ARGTYPE(ARG_NUM),    "align",        &domNumAlign },
  { ARGTYPE(ARG_NUM),    "pattern",      &domNumFillPattern } };
// Argument profile for paint set draw color
cmdArg_t argPaintSetColor[] =
{ { ARGTYPE(ARG_NUM),    "color",        &domNumColor } };

// Command 'r*'
// Argument profile for repeat for
cmdArg_t argRepeatFor[] =
{ { ARGTYPE(ARG_NUM),    "init",         &domNumInit },
  { ARGTYPE(ARG_NUM),    "condition",    &domNumConditionBlock },
  { ARGTYPE(ARG_NUM),    "post",         &domNumPost } };

// Command 's*'
// Argument profile for providing command stack statistics
cmdArg_t argStatsStack[] =
{ { ARGTYPE(ARG_NUM),    "enable",       &domNumOffOn } };

// Command 't*'
// Argument profile for alarm switch position
cmdArg_t argTimeAlarmPos[] =
{ { ARGTYPE(ARG_NUM),    "position",     &domNumOffOn } };
// Argument profile for set alarm
cmdArg_t argTimeAlarmSet[] =
{ { ARGTYPE(ARG_NUM),    "hour",         &domNumHour },
  { ARGTYPE(ARG_NUM),    "min",          &domNumMinSec } };
// Argument profile for date set
cmdArg_t argTimeDateSet[] =
{ { ARGTYPE(ARG_NUM),    "day",          &domNumDay },
  { ARGTYPE(ARG_NUM),    "month",        &domNumMonth },
  { ARGTYPE(ARG_NUM),    "year",         &domNumYear } };
// Argument profile for time get
cmdArg_t argTimeGet[] =
{ { ARGTYPE(ARG_STRING), "varyear",      &domStrVarName },
  { ARGTYPE(ARG_STRING), "varmonth",     &domStrVarName },
  { ARGTYPE(ARG_STRING), "varday",       &domStrVarName },
  { ARGTYPE(ARG_STRING), "varhour",      &domStrVarName },
  { ARGTYPE(ARG_STRING), "varmin",       &domStrVarName },
  { ARGTYPE(ARG_STRING), "varsec",       &domStrVarName } };

// Argument profile for time set
cmdArg_t argTimeSet[] =
{ { ARGTYPE(ARG_NUM),    "hour",         &domNumHour },
  { ARGTYPE(ARG_NUM),    "min",          &domNumMinSec },
  { ARGTYPE(ARG_NUM),    "sec",          &domNumMinSec } };

// Command 'v*'
// Argument profile for variable print
cmdArg_t argVarPrint[] =
{ { ARGTYPE(ARG_STRING), "pattern",      &domStrVarPattern } };
// Argument profile for variable reset
cmdArg_t argVarReset[] =
{ { ARGTYPE(ARG_STRING), "variable",     &domStrVarNameAll } };
// Argument profile for variable value set
cmdArg_t argVarSet[] =
{ { ARGTYPE(ARG_NUM),    "assignment",   &domNumAssign } };

// Command 'w*'
// Argument profile for wait
cmdArg_t argWait[] =
{ { ARGTYPE(ARG_NUM),    "delay",        &domNumDelay } };
// Argument profile for timer expiry
cmdArg_t argWaitTimerExpiry[] =
{ { ARGTYPE(ARG_NUM),    "expiry",       &domNumExpiry } };

// Command 'x*'
// No additional profiles are needed

//
// Dictionary build-up step 3: command group profiles
//
// This is about combining commands in a command group.
// WARNING: Commands in a command group profile MUST be kept in alphabetical
// order due to the linear command search method as well as printing in proper
// alphabetical order using mchron command 'hc'. This is verified at mchron
// startup.
//

// All commands for command group '#' (comments)
cmdCommand_t cmdGroupComments[] =
{ { "#",   PCBTYPE(PCB_CONTINUE),    MC_FALSE, CMDARGS(argComments),        CMDHANDLER(doComments),        "comments" } };

// All commands for command group 'b' (beep)
cmdCommand_t cmdGroupBeep[] =
{ { "b",   PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argBeep),            CMDHANDLER(doBeep),            "play beep" } };

// All commands for command group 'c' (clock)
cmdCommand_t cmdGroupClock[] =
{ { "cf",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argClockFeed),       CMDHANDLER(doClockFeed),       "feed clock time/keyboard events" },
  { "cp",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doClockPrint),      "print available clocks" },
  { "cs",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argClockSelect),     CMDHANDLER(doClockSelect),     "select clock" } };

// All commands for command group 'd' (debug)
cmdCommand_t cmdGroupDebug[] =
{ { "dba", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argDbBpAdd),         CMDHANDLER(doDbBpAdd),         "add/update breakpoint" },
  { "dbd", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argDbBpDelete),      CMDHANDLER(doDbBpDelete),      "delete breakpoint" },
  { "dbp", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doDbBpPrint),       "print breakpoints" },
  { "dc",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doDbContinue),      "debug continue" },
  { "dcc", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argDbBpCond),        CMDHANDLER(doDbBpCond),        "check conditional breakpoint" },
  { "di",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doDbStepIn),        "debug step in" },
  { "dn",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doDbStepNext),      "debug step next" },
  { "do",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doDbStepOut),       "debug step out" },
  { "dps", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argDbPcSet),         CMDHANDLER(doDbPcSet),         "set top stack program counter" },
  { "dss", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argDbSet),           CMDHANDLER(doDbSet),           "set script debugging" } };

// All commands for command group 'e' (execute)
cmdCommand_t cmdGroupExecute[] =
{ { "e",   PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argExecute),         CMDHANDLER(doExecFile),        "execute commands from file" },
  { "elp", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argExecListPrint),   CMDHANDLER(doExecListPrint),   "print source list" },
  { "elr", PCBTYPE(PCB_RETURN),      MC_TRUE,  CMDARGS(NULL),               PCCTRLHANDLER(doExecReturn),   "return from current stack level" },
  { "er",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doExecResume),      "resume interrupted execution" },
  { "esp", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doExecStackPrint),  "print command stack" } };

// All commands for command group 'g' (graphics buffer)
cmdCommand_t cmdGroupGraphics[] =
{ { "gbc", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argGrCopy),          CMDHANDLER(doGrCopy),          "copy graphics buffer" },
  { "gbi", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argGrInfo),          CMDHANDLER(doGrInfo),          "show graphics buffer info" },
  { "gbr", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argGrReset),         CMDHANDLER(doGrReset),         "reset graphics buffer" },
  { "gbs", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argGrSaveFile),      CMDHANDLER(doGrSaveFile),      "save graphics buffer to file" },
  { "gci", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argGrLoadCtrImg),    CMDHANDLER(doGrLoadCtrImg),    "load controller lcd image data" },
  { "gf",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argGrLoadFile),      CMDHANDLER(doGrLoadFile),      "load file graphics data" },
  { "gfi", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argGrLoadFileImg),   CMDHANDLER(doGrLoadFileImg),   "load file image data" },
  { "gfs", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argGrLoadFileSpr),   CMDHANDLER(doGrLoadFileSpr),   "load file sprite data" } };

// All commands for command group 'h' (help)
cmdCommand_t cmdGroupHelp[] =
{ { "h",   PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doHelp),            "show help" },
  { "hc",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argHelpCmd),         CMDHANDLER(doHelpCmd),         "search command" },
  { "he",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argHelpExpr),        CMDHANDLER(doHelpExpr),        "show expression result" },
  { "hm",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argHelpMsg),         CMDHANDLER(doHelpMsg),         "show help message" } };

// All commands for command group 'i' (if)
cmdCommand_t cmdGroupIf[] =
{ { "iei", PCBTYPE(PCB_IF_ELSE_IF),  MC_TRUE,  CMDARGS(argIfElseIf),        PCCTRLHANDLER(doIfElseIf),     "if else if" },
  { "iel", PCBTYPE(PCB_IF_ELSE),     MC_TRUE,  CMDARGS(NULL),               PCCTRLHANDLER(doIfElse),       "if else" },
  { "ien", PCBTYPE(PCB_IF_END),      MC_TRUE,  CMDARGS(NULL),               PCCTRLHANDLER(doIfEnd),        "if end" },
  { "iif", PCBTYPE(PCB_IF),          MC_TRUE,  CMDARGS(argIf),              PCCTRLHANDLER(doIf),           "if" } };

// All commands for command group 'l' (lcd)
cmdCommand_t cmdGroupLcd[] =
{ { "lbs", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argLcdBacklightSet), CMDHANDLER(doLcdBacklightSet), "set lcd backlight brightness" },
  { "lcr", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doLcdCursorReset),  "reset lcd cursor administration" },
  { "lcs", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argLcdActCtrlSet),   CMDHANDLER(doLcdActCtrlSet),   "set active lcd controller" },
  { "lds", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argLcdDisplaySet),   CMDHANDLER(doLcdDisplaySet),   "switch lcd controller display on/off" },
  { "le",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doLcdErase),        "erase lcd display" },
  { "lge", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doLcdGlutEdit),     "edit glut lcd display" },
  { "lgg", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argLcdGlutGrSet),    CMDHANDLER(doLcdGlutGrSet),    "set glut graphics options" },
  { "lgs", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argLcdGlutSizeSet),  CMDHANDLER(doLcdGlutSizeSet),  "set glut window size" },
  { "lhr", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doLcdGlutHlReset),  "reset glut glcd pixel highlight" },
  { "lhs", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argLcdGlutHlSet),    CMDHANDLER(doLcdGlutHlSet),    "set glut glcd pixel highlight" },
  { "li",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doLcdInverse),      "inverse lcd display and draw colors" },
  { "lng", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argLcdNcurGrSet),    CMDHANDLER(doLcdNcurGrSet),    "set ncurses graphics options" },
  { "lp",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doLcdPrint),        "print lcd controller state/registers" },
  { "lr",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argLcdRead),         CMDHANDLER(doLcdRead),         "read data from active lcd controller" },
  { "lss", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argLcdStartLineSet), CMDHANDLER(doLcdStartLineSet), "set lcd controller start line" },
  { "lw",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argLcdWrite),        CMDHANDLER(doLcdWrite),        "write data to active lcd controller" },
  { "lxs", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argLcdXCursorSet),   CMDHANDLER(doLcdXCursorSet),   "set active lcd controller x cursor" },
  { "lys", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argLcdYCursorSet),   CMDHANDLER(doLcdYCursorSet),   "set active lcd controller y cursor" } };

// All commands for command group 'm' (monochron)
cmdCommand_t cmdGroupMonochron[] =
{ { "m",   PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argMonochron),       CMDHANDLER(doMonochron),       "run monochron application" },
  { "mc",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argMonoConfig),      CMDHANDLER(doMonoConfig),      "run monochron config" },
  { "mep", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doEepromPrint),     "print monochron eeprom settings" },
  { "mer", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doEepromReset),     "reset monochron eeprom" },
  { "mew", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argEepromWrite),     CMDHANDLER(doEepromWrite),     "write data to monochron eeprom" } };

// All commands for command group 'p' (paint)
cmdCommand_t cmdGroupPaint[] =
{ { "pa",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argPaintAscii),      CMDHANDLER(doPaintAscii),      "paint ascii" },
  { "pb",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argPaintBuffer),     CMDHANDLER(doPaintBuffer),     "paint buffer" },
  { "pbi", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argPaintBufferImg),  CMDHANDLER(doPaintBufferImg),  "paint buffer image" },
  { "pbs", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argPaintBufferSpr),  CMDHANDLER(doPaintBufferSpr),  "paint buffer sprite" },
  { "pc",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argPaintCircle),     CMDHANDLER(doPaintCircle),     "paint circle" },
  { "pcf", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argPaintCircleFill), CMDHANDLER(doPaintCircleFill), "paint filled circle" },
  { "pd",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argPaintDot),        CMDHANDLER(doPaintDot),        "paint dot" },
  { "pl",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argPaintLine),       CMDHANDLER(doPaintLine),       "paint line" },
  { "pn",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argPaintNumber),     CMDHANDLER(doPaintNumber),     "paint number" },
  { "pr",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argPaintRect),       CMDHANDLER(doPaintRect),       "paint rectangle" },
  { "prf", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argPaintRectFill),   CMDHANDLER(doPaintRectFill),   "paint filled rectangle" },
  { "ps",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argPaintSetColor),   CMDHANDLER(doPaintSetColor),   "set draw color" },
  { "psb", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doPaintSetBg),      "set draw color to background color" },
  { "psf", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doPaintSetFg),      "set draw color to foreground color" } };

// All commands for command group 'r' (repeat)
cmdCommand_t cmdGroupRepeat[] =
{ { "rb",  PCBTYPE(PCB_REPEAT_BRK),  MC_TRUE,  CMDARGS(NULL),               PCCTRLHANDLER(doRepeatBreak),  "repeat break" },
  { "rc",  PCBTYPE(PCB_REPEAT_CONT), MC_TRUE,  CMDARGS(NULL),               PCCTRLHANDLER(doRepeatCont),   "repeat continue" },
  { "rf",  PCBTYPE(PCB_REPEAT_FOR),  MC_TRUE,  CMDARGS(argRepeatFor),       PCCTRLHANDLER(doRepeatFor),    "repeat for" },
  { "rn",  PCBTYPE(PCB_REPEAT_NEXT), MC_TRUE,  CMDARGS(NULL),               PCCTRLHANDLER(doRepeatNext),   "repeat next" } };

// All commands for command group 's' (statistics)
cmdCommand_t cmdGroupStats[] =
{ { "sls", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argStatsStack),      CMDHANDLER(doStatsStack),      "set list runtime statistics" },
  { "sp",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doStatsPrint),      "print application statistics" },
  { "sr",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doStatsReset),      "reset application statistics" } };

// All commands for command group 't' (time/date/alarm)
cmdCommand_t cmdGroupTime[] =
{ { "tap", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argTimeAlarmPos),    CMDHANDLER(doTimeAlarmPos),    "set alarm switch position" },
  { "tas", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argTimeAlarmSet),    CMDHANDLER(doTimeAlarmSet),    "set alarm time" },
  { "tat", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doTimeAlarmToggle), "toggle alarm switch position" },
  { "tdr", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doTimeDateReset),   "reset date to system date" },
  { "tds", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argTimeDateSet),     CMDHANDLER(doTimeDateSet),     "set date" },
  { "tf",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doTimeFlush),       "flush time/date to clock" },
  { "tg",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argTimeGet),         CMDHANDLER(doTimeGet),         "get date/time" },
  { "tp",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doTimePrint),       "print time/date/alarm" },
  { "tr",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doTimeReset),       "reset time to system time" },
  { "ts",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argTimeSet),         CMDHANDLER(doTimeSet),         "set time" } };

// All commands for command group 'v' (variable)
cmdCommand_t cmdGroupVar[] =
{ { "vp",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argVarPrint),        CMDHANDLER(doVarPrint),        "print value variable(s)" },
  { "vr",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argVarReset),        CMDHANDLER(doVarReset),        "reset value variable(s)" },
  { "vs",  PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argVarSet),          CMDHANDLER(doVarSet),          "set value variable" } };

// All commands for command group 'w' (wait)
cmdCommand_t cmdGroupWait[] =
{ { "w",   PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argWait),            CMDHANDLER(doWait),            "wait for keypress or time" },
  { "wte", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(argWaitTimerExpiry), CMDHANDLER(doWaitTimerExpiry), "wait for wait timer expiry" },
  { "wts", PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doWaitTimerStart),  "start wait timer" } };

// All commands for command group 'x' (exit)
cmdCommand_t cmdGroupExit[] =
{ { "x",   PCBTYPE(PCB_CONTINUE),    MC_TRUE,  CMDARGS(NULL),               CMDHANDLER(doExit),            "exit mchron" } };

//
// Dictionary build-up step 4: the complete mchron command dictionary and its size
//
// This is about merging the command groups into the final command dictionary
//
cmdDict_t cmdDictMchron[] =
{
  { '#', "comments",   CMDGROUP(cmdGroupComments) },
  { 'a', "-",          CMDGROUP(NULL) },
  { 'b', "beep",       CMDGROUP(cmdGroupBeep) },
  { 'c', "clock",      CMDGROUP(cmdGroupClock) },
  { 'd', "debug",      CMDGROUP(cmdGroupDebug) },
  { 'e', "execute",    CMDGROUP(cmdGroupExecute) },
  { 'f', "-",          CMDGROUP(NULL) },
  { 'g', "graphics",   CMDGROUP(cmdGroupGraphics) },
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
