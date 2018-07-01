//*****************************************************************************
// Filename : 'genalarm.c'
// Title    : Utility tool to generate Monochron alarm audio file
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Emuchron defines
#include "stub.h"

// Load alarm info and Mario alarm data (when configured)
#include "../mariotune.h"

// Templates for sox command
#define SOX_SILENT	"'|/usr/bin/sox -b16 -r12k -Dnp synth %f sin 0' "
#define SOX_TONE	"'|/usr/bin/sox -b16 -r12k -Dnp synth %f sin %d' "

// This is me
extern const char *__progname;

//
// Function: main
//
// Main program for generating an mchron alarm audio file, based on whether
// Mario or two-tone alarm is configured in alarm.h [firmware].
// It requires a single argument, being the target audio filename, that must
// have suffix ".au".
//
// Example shell command to play the generated audio file (use ^C to quit):
// play -q emulator/alarm.au -t alsa repeat 100
//
int main(int argc, char *argv[])
{
  char *soxCmd = 0;
  int paramLen = 0;
  int paramCount = 0;
  int paramReq = 0;
#ifdef MARIO
  int i = 0;
  int j = 0;
  int lineStart = 0;
  int lineLength = 0;
#endif

  // Check for single argument
  if (argc != 2)
  {
    printf("%s: require single audio filename (*.au) argument\n", __progname);
    return -1;
  }
  //printf("target audio filename: %s\n", argv[1]);

  // Preparations for generating the appropriate sox command
  paramReq = 180 + strlen(argv[1]);
#ifdef MARIO
  // Mario alarm
  if (sizeof(MarioTones) != sizeof(MarioBeats))
  {
    printf("%s: mario tones and beats sizes are not aligned: %ld %ld\n",
      __progname, (long int)sizeof(MarioTones), (long int)sizeof(MarioBeats));
    return -1;
  }
  //printf("tones=%ld, lines=%ld\n", sizeof(MarioTones),
  //  sizeof(MarioMaster) / 2);

  // Make decent approximation of target command length for Mario alarm
  for (i = 0; i < marioMasterLen; i = i + 2)
  {
    lineLength = MarioMaster[i + 1];
    paramReq = paramReq + lineLength * (strlen(SOX_SILENT) + 7) +
      lineLength * (strlen(SOX_TONE) + 9);
  }
#else
  // Two-tone alarm

  // Make decent approximation of target command length for two-tone alarm
  paramReq = paramReq + 2 * (strlen(SOX_SILENT) + 7) +
    2 * (strlen(SOX_TONE) + 9);
#endif

  // Allocate enough heap space to store our target sox command
  soxCmd = (char *)malloc(paramReq);

  // Let's create some audio using execlp
  paramLen = paramLen + sprintf(soxCmd + paramLen,
    "/usr/bin/sox --combine concatenate ");
  paramCount++;

  // Generate the sequence of sox audio commands
#ifdef MARIO
  // The Mario chiptune tones
  for (i = 0; i < marioMasterLen; i = i + 2)
  {
    lineStart = MarioMaster[i];
    lineLength = MarioMaster[i + 1];
    for (j = lineStart; j < lineStart + lineLength; j++)
    {
      // The tone using a beat byte
      paramLen = paramLen + sprintf(soxCmd + paramLen, SOX_TONE,
        (float)(MarioBeats[j] * MAR_TEMPO / MAR_BEAT_FACTOR) / 1000,
        MarioTones[j] * MAR_TONE_FACTOR);
      paramCount++;

      // Add a pauze of half a beat between tones
      paramLen = paramLen + sprintf(soxCmd + paramLen, SOX_SILENT,
        (float)(MAR_TEMPO / 2) / 1000);
      paramCount++;
    }
  }
#else
  // Two-tone alarm tones
  paramLen = paramLen + sprintf(soxCmd + paramLen, SOX_TONE,
    (float)SND_TICK_TONE_MS / 1000, ALARM_FREQ_1);
  paramCount++;
  paramLen = paramLen + sprintf(soxCmd + paramLen, SOX_SILENT,
    (float)SND_TICK_TONE_MS / 1000);
  paramCount++;
  paramLen = paramLen + sprintf(soxCmd + paramLen, SOX_TONE,
    (float)SND_TICK_TONE_MS / 1000, ALARM_FREQ_2);
  paramCount++;
  paramLen = paramLen + sprintf(soxCmd + paramLen, SOX_SILENT,
    (float)SND_TICK_TONE_MS / 1000);
  paramCount++;
#endif

  // The last item is the target audio output file
  paramLen = paramLen + sprintf(soxCmd + paramLen, "%s", argv[1]);
  paramCount++;

  // Statistics
  //printf("\ngenerated sox command:\n%s\n\n", soxCmd);
  //printf("count=%d, len=%d, req=%d\n\n", paramCount, paramLen, paramReq);

  // A feeble attempt to see if we miscalculated the expected heap space
  if (paramLen >= paramReq)
  {
    printf("%s: insufficient sox command space allocated: %d %d\n", __progname,
      paramLen, paramReq);
    return -1;
  }

  // Execute the sox command we just created resulting in the target audio file
  execlp("/bin/sh", "/bin/sh", "-c", soxCmd, (char *)NULL);
  free(soxCmd);

  return 0;
}
