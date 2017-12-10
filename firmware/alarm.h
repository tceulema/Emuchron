//*****************************************************************************
// Filename : 'alarm.h'
// Title    : Config and defs for alarm and Mario melody or two-tone tune
//*****************************************************************************

#ifndef ALARM_H
#define ALARM_H

// Set timeouts for snooze and alarm (in seconds)
#ifndef EMULIN
#define ALM_TICK_SNOOZE_SEC	600
#define ALM_TICK_ALARM_SEC	1800
#else
// In our emulator we don't want to wait that long
#define ALM_TICK_SNOOZE_SEC	25
#define ALM_TICK_ALARM_SEC	65
#endif

// Uncomment this if you want a Mario tune alarm instead of a two-tone alarm.
// Note: This will cost you ~536 bytes of Monochron program and data space.
#define MARIO

#ifdef MARIO
// Configure Mario melody alarm
// The tune tempo
#define MAR_TEMPO		99
// The factors used to fit tone and beat values in an unsigned char data
// type. The densed values are expanded using these very same factors.
// DO NOT CHANGE THESE as they require a rebuild of respectively MarioTones[]
// and MarioBeats[] contents.
#define MAR_TONE_FACTOR		9
#define MAR_BEAT_FACTOR		3
#else
// Configure two-tone alarm
#define ALARM_FREQ_1		4000
#define ALARM_FREQ_2		3750
#define SND_TICK_TONE_MS	325
#endif
#endif
