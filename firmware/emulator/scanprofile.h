//*****************************************************************************
// Filename : 'scanprofile.h'
// Title    : Command line tool scan profiles for the MONOCHRON emulator
//*****************************************************************************

#ifndef SCANPROFILE_H
#define SCANPROFILE_H

#include "../ks0108conf.h"
#include "../glcd.h"
#include "scanutil.h"

//
// Below are argument domain value profiles
//

// Alarm switch position: 0..1
argItemDomain_t argNumSwitchPosition =
{ DOM_NUM_MAX, "", 0, 1 };

// Beep duration 1..255
argItemDomain_t argNumDuration =
{ DOM_NUM_RANGE, "", 1, 255 };

// Beep frequency 150..10000
argItemDomain_t argNumFrequency =
{ DOM_NUM_RANGE, "", 150, 10000 };

// Circle draw pattern: max 3
argItemDomain_t argNumCirclePattern =
{ DOM_NUM_MAX, "", 0, 3 };

// Circle/rectangle fill pattern: max 5
argItemDomain_t argNumFillPattern =
{ DOM_NUM_MAX, "", 0, 5 };

// Circle radius: max 31
argItemDomain_t argNumRadius =
{ DOM_NUM_MAX, "", 0, 31 };

// Date day: max 31
argItemDomain_t argNumDay =
{ DOM_NUM_RANGE, "", 1, 31 };

// Date month: max 12
argItemDomain_t argNumMonth =
{ DOM_NUM_RANGE, "", 1, 12 };

// Date year: max 99
argItemDomain_t argNumYear =
{ DOM_NUM_MAX, "", 0, 99 };

// Draw x size: 1..128
argItemDomain_t argNumXSize =
{ DOM_NUM_RANGE, "", 1, GLCD_XPIXELS };

// Draw y size: 1..64
argItemDomain_t argNumYSize =
{ DOM_NUM_RANGE, "", 1, GLCD_YPIXELS };

// Echo command: 'e'cho, 'i'nherit, 's'ilent
argItemDomain_t argCharEcho =
{ DOM_CHAR_LIST, "eis", 0, 0 };

// Emulator start mode: 'c'ycle mode or 'n'ormal mode
argItemDomain_t argCharMode =
{ DOM_CHAR_LIST, "cn", 0, 0 };

// Emulator eeprom use: 'k'eep or 'r'eset
argItemDomain_t argCharEeprom =
{ DOM_CHAR_LIST, "kr", 0, 0 };

// LCD backlight: max 16
argItemDomain_t argNumBacklight =
{ DOM_NUM_MAX, "", 0, 16 };

// LCD color: 'b'ackground or 'f'oreground
argItemDomain_t argCharColor =
{ DOM_CHAR_LIST, "bf", 0, 0 };

// LCD x position: max 127
argItemDomain_t argNumPosX =
{ DOM_NUM_MAX, "", 0, GLCD_XPIXELS - 1 };

// LCD y position: max 63
argItemDomain_t argNumPosY =
{ DOM_NUM_MAX, "", 0, GLCD_YPIXELS - 1 };

// Rectangle fill align: 0..2
argItemDomain_t argNumAlign =
{ DOM_NUM_MAX, "", 0, 2 };

// Text font: 5x5-proportional, 5x7-non-proportional
argItemDomain_t argWordFont =
{ DOM_WORD_LIST, "5x5p\n5x7n", 0, 0 };

// Text orientation: 'b'ottom-up, 'h'orizontal, 't'op-down
argItemDomain_t argCharOrient =
{ DOM_CHAR_LIST, "bht", 0, 0 };

// Text scale: min 1
argItemDomain_t argNumScale =
{ DOM_NUM_MIN, "", 1, 0 };

// Time hour: max 23
argItemDomain_t argNumHour =
{ DOM_NUM_MAX, "", 0, 23 };

// Time minute/second: max 59
argItemDomain_t argNumMinSec =
{ DOM_NUM_MAX, "", 0, 59 };

// Wait delay: max 100K
argItemDomain_t argNumDelay =
{ DOM_NUM_MAX, "", 0, 100000 };

//
// Below are command line argument scan profiles
//

// Generic profile for argument command
// This profile is used as a step 1 prefix scan for all other profiles
argItem_t argCmd[] =
{ { ARG_WORD,   "command",     0 } };

// Generic profile for end of line argument
// This profile is used by commands that do not require any arguments
argItem_t argEnd[] =
{ { ARG_END,    "",            0 } };

// Command '#'
// No additional profiles are needed

// Command 'a*'
// Additional argument item profile for alarm switch position
argItem_t argAlarmPosition[] =
{ { ARG_UINT,   "position",    &argNumSwitchPosition },
  { ARG_END,    "",            0 } };
// Additional argument item profile for set alarm
argItem_t argAlarmSet[] =
{ { ARG_UINT,   "hour",        &argNumHour },
  { ARG_UINT,   "min",         &argNumMinSec },
  { ARG_END,    "",            0 } };

// Command 'b*'
// Additional argument item profile for beep
argItem_t argBeep[] =
{ { ARG_UINT,   "frequency",   &argNumFrequency },
  { ARG_UINT,   "duration",    &argNumDuration },
  { ARG_END,    "",            0 } };

// Command 'c*'
// Additional argument profile for clock feed
argItem_t argClockFeed[] =
{ { ARG_CHAR,   "mode",        &argCharMode },
  { ARG_END,    "",            0 } };
// Additional argument profile for clock select
argItem_t argClockSelect[] =
{ { ARG_UINT,   "clock",       0 },
  { ARG_END,    "",            0 } };

// Command 'd*'
// Additional argument profile for date set
argItem_t argDateSet[] =
{ { ARG_UINT,   "day",         &argNumDay },
  { ARG_UINT,   "month",       &argNumMonth },
  { ARG_UINT,   "year",        &argNumYear },
  { ARG_END,    "",            0 } };

// Command 'e'
// Additional argument profile for execute command file
argItem_t argExecute[] =
{ { ARG_CHAR,   "echo",        &argCharEcho },
  { ARG_STRING, "filename",    0 },
  { ARG_END,    "",            0 } };

// Command 'h'
// No additional profiles are needed

// Command 'l*'
// Additional argument profile for LCD backlight set
argItem_t argBacklight[] =
{ { ARG_UINT,   "backlight",   &argNumBacklight },
  { ARG_END,    "",            0 } };

// Command 'm'
// Additional argument profile for Monochron emulator
argItem_t argMchronStart[] =
{ { ARG_CHAR,   "mode",        &argCharMode },
  { ARG_CHAR,   "eeprom",      &argCharEeprom },
  { ARG_END,    "",            0 } };

// Command 'p*'
// Additional argument profile for paint ascii
argItem_t argPaintAscii[] =
{ { ARG_CHAR,   "color",       &argCharColor },
  { ARG_UINT,   "x",           &argNumPosX },
  { ARG_UINT,   "y",           &argNumPosY },
  { ARG_WORD,   "font",        &argWordFont },
  { ARG_CHAR,   "orientation", &argCharOrient },
  { ARG_UINT,   "xscale",      &argNumScale },
  { ARG_UINT,   "yscale",      &argNumScale },
  { ARG_STRING, "text",        0 } };
// Additional argument profile for paint circle
argItem_t argPaintCircle[] =
{ { ARG_CHAR,   "color",       &argCharColor },
  { ARG_UINT,   "x",           &argNumPosX },
  { ARG_UINT,   "y",           &argNumPosY },
  { ARG_UINT,   "radius",      &argNumRadius },
  { ARG_UINT,   "pattern",     &argNumCirclePattern},
  { ARG_END,    "",            0 } };
// Additional argument profile for paint circle filled
argItem_t argPaintCircleFill[] =
{ { ARG_CHAR,   "color",       &argCharColor },
  { ARG_UINT,   "x",           &argNumPosX },
  { ARG_UINT,   "y",           &argNumPosY },
  { ARG_UINT,   "radius",      &argNumRadius },
  { ARG_UINT,   "pattern",     &argNumFillPattern },
  { ARG_END,    "",            0 } };
// Additional argument profile for paint dot
argItem_t argPaintDot[] =
{ { ARG_CHAR,   "color",       &argCharColor },
  { ARG_UINT,   "x",           &argNumPosX },
  { ARG_UINT,   "y",           &argNumPosY },
  { ARG_END,    "",            0 } };
// Additional argument profile for paint line
argItem_t argPaintLine[] =
{ { ARG_CHAR,   "color",       &argCharColor },
  { ARG_UINT,   "xstart",      &argNumPosX },
  { ARG_UINT,   "ystart",      &argNumPosY },
  { ARG_UINT,   "xend",        &argNumPosX },
  { ARG_UINT,   "yend",        &argNumPosY },
  { ARG_END,    "",            0 } };
// Additional argument profile for paint rectangle
argItem_t argPaintRect[] =
{ { ARG_CHAR,   "color",       &argCharColor },
  { ARG_UINT,   "x",           &argNumPosX },
  { ARG_UINT,   "y",           &argNumPosY },
  { ARG_UINT,   "xsize",       &argNumXSize },
  { ARG_UINT,   "ysize",       &argNumYSize },
  { ARG_END,    "",            0 } };
// Additional argument profile for paint rectangle filled
argItem_t argPaintRectFill[] =
{ { ARG_CHAR,   "color",       &argCharColor },
  { ARG_UINT,   "x",           &argNumPosX },
  { ARG_UINT,   "y",           &argNumPosY },
  { ARG_UINT,   "xsize",       &argNumXSize },
  { ARG_UINT,   "ysize",       &argNumYSize },
  { ARG_UINT,   "align",       &argNumAlign },
  { ARG_UINT,   "pattern",     &argNumFillPattern },
  { ARG_END,    "",            0 } };

// Command 'r*'
// Full profiles for repeat commands are locally defined in mchronutil.c

// Command 's*'
// No additional profiles are needed

// Command 't*'
// Additional argument profile for time set
argItem_t argTimeSet[] =
{ { ARG_UINT,   "hour",        &argNumHour },
  { ARG_UINT,   "min",         &argNumMinSec },
  { ARG_UINT,   "sec",         &argNumMinSec },
  { ARG_END,    "",            0 } };

// Command 'v*'
// Additional argument profile for variable print
argItem_t argVarPrint[] =
{ { ARG_WORD,   "variable",    0 },
  { ARG_END,    "",            0 } };
// Additional argument profile for variable reset
argItem_t argVarReset[] =
{ { ARG_WORD,   "variable",    0 },
  { ARG_END,    "",            0 } };
// Additional argument profile for variable value set
argItem_t argVarSet[] =
{ { ARG_WORD,   "variable",    0 },
  { ARG_INT,    "value",       0 },
  { ARG_END,    "",            0 } };

// Command 'w'
// Additional argument profile for wait
argItem_t argWait[] =
{ { ARG_UINT,   "delay",       &argNumDelay },
  { ARG_END,    "",            0 } };

// Command 'x'
// No additional profiles are needed
#endif
