//
// For background info on this utility program refer to the definition of
// progmem array yDelta in wave.c [firmware/clock].
// Build this program using: gcc -g wavesin.c -o wavesin -lm
// Execute this program using: ./wavesin
//
#include <stdio.h>
#include <math.h>

#define WAVE_LEN	120
#define WAVE_START_Y	1
#define WAVE_DELTA_Y	13

int main()
{
  int i;
  int yDelta;

  for (i = 0; i < WAVE_LEN; i++)
  {
    if (i % 15 == 0)
      printf("\n  ");
    yDelta = (int)roundf((float)WAVE_DELTA_Y + WAVE_DELTA_Y * sin(2 * M_PI * i /
      WAVE_LEN)) + WAVE_START_Y;
    printf("0x%02x", yDelta);
    if (i < WAVE_LEN - 1)
      printf(",");
  }
  printf("\n");

  return 0;
}
