//*****************************************************************************
// Filename : 'mariotune.h'
// Title    : Raw piezo data for playing the world famous Mario melody
//*****************************************************************************

// The tone and beat data is based on the following cute Arduino project:
// http://www.youtube.com/watch?v=VqeYvJpibLY

#ifndef MARIOTUNE_H
#define MARIOTUNE_H

#include "mario.h"

//
// The Arduino firmware stores the 295 notes as a char array and maps each note
// to a tone via a linear search algorithm in two other arrays. This is considered
// too CPU intensive as our code runs in a 1 msec interrupt handler.
// So, we are going to store the notes as tones. Two things need to be taken of.
// First, the Monochron piezo is not able to properly reproduce low frequency
// tones. We have to add one octave to the original tones to make the audio
// audible anyway.
// Second, we need tones over the 255 Hz range, and to be able to store these
// values we need 16 bit integers. To reduce the data space footprint we want to
// put a tone in a single byte.
// To meet both challenges, a Mario note from the Arduino firmware is converted
// into a base tone and is increased 1 octave. To allow storing the resulting tone
// in a single byte it is divided by MAR_TONEFACTOR. The densed tone value is
// rounded upwards or downwards to minimize the delta upon reproducing its value.
// This means that a reproduced tone (= tone * MAR_TONEFACTOR) is off by at most
// MAR_TONEFACTOR / 2 = +/-4 Hz.
// Since the quality of the piezo speaker in Monochron is worse than horrible,
// this 4 Hz delta sounds (pun intended) acceptable to me.
//
// Note: The original linear tone search algorithm overhead is replaced by fixed
// length basic integer arithmetic to reproduce a tone. It is considered to be an
// acceptable alternative. Also, we no longer need the search tables either, saving
// a rough 100 bytes on data space.
// Note: Each byte in this array must have a corresponding byte in MarioBeats[].
//
const unsigned char __attribute__ ((progmem)) MarioTones[] =
{ 146, 146, 0, 146, 0, 116, 146, 0, 174, 0, 0, 87, 0, 0, 116, 0, 0, 87, 0, 73, 0, 0, 98,
0, 110, 0, 104, 98, 0, 87, 146, 174, 196, 0, 155, 174, 0, 146, 0, 116, 130, 110, 0, 116,
0, 0, 87, 0, 73, 0, 0, 98, 0, 110, 0, 104, 98, 0, 87, 146, 174, 196, 0, 155, 174, 0,
146, 0, 116, 130, 110, 0, 0, 174, 164, 155, 138, 0, 146, 0, 92, 98, 116, 0, 98, 116,
130, 0, 174, 164, 155, 138, 0, 146, 0, 232, 0, 232, 232, 0, 0, 0, 174, 164, 155, 138, 0,
146, 0, 92, 98, 116, 0, 98, 116, 130, 0, 138, 0, 0, 130, 0, 116, 0, 0, 0, 116, 116, 0,
116, 0, 116, 130, 0, 146, 116, 0, 98, 87, 0, 0, 116, 116, 0, 116, 0, 116, 130, 146, 0,
0, 116, 116, 0, 116, 0, 116, 130, 0, 146, 116, 0, 98, 87, 0, 0, 146, 146, 0, 146, 0,
116, 146, 0, 174, 0, 0, 87, 0, 0, 116, 0, 0, 87, 0, 73, 0, 0, 98, 0, 110, 0, 104, 98,
0, 87, 146, 174, 196, 0, 155, 174, 0, 146, 0, 116, 130, 110, 0, 116, 0, 0, 87, 0, 73, 0,
0, 98, 0, 110, 0, 104, 98, 0, 87, 146, 174, 196, 0, 155, 174, 0, 146, 0, 116, 130, 110,
0, 146, 116, 0, 87, 0, 92, 0, 98, 155, 0, 155, 98, 0, 0, 110, 196, 196, 196, 174, 155,
146, 116, 0, 98, 87, 0, 0, 146, 116, 0, 87, 0, 92, 0, 98, 155, 0, 155, 98, 0, 0, 110,
155, 0, 155, 155, 146, 130, 116, 73, 0, 73, 58, 0, 0, 0, 0 };

//
// The Arduino firmware stores the 295 beats as floats, which is 4 bytes per beat.
// To reduce the program space footprint and to avoid the use of floating point
// arithmetic we want put a single beat in a byte. A densed Mario beat is put in a
// byte using division factor MAR_BEATFACTOR. This approach saves us CPU expensive
// floating point arithmetic and... 885 bytes of data space!
// Concerning inaccuracies in reproducing durations, here's the worst-case scenario:
// When in the Arduino data a beat duration 1.3 is used this leads to a duration of
// 1.3 * MAR_TEMPO = 123 msec. Our implementation for the same beat in a byte will
// lead to a duration of 4 * MAR_TEMPO / MAR_BEATFACTOR = 126 msec. Not bad.
// Since the quality of the piezo speaker in Monochron is worse than horrible
// this 3 msec delta sounds (pun intended) acceptable to me.
//
// Note: Yes, the floating point arithmetic is replaced by a slightly more
// complicated integer algorithm to expand a byte value. The integer algorithm is
// expected to be much faster than its floating point counterpart.
// Note: Looking at the data below we can even put each value in a nibble, saving
// an additional 295 / 2 = 147 data space bytes. However, the algorithm to expand
// the nibble back to its value becomes more CPU intensive and will cost approx. 70
// extra bytes of runtime code, making the total savings only 75 bytes. Considering
// the additional CPU strain in the 1 msec interrupt handler, this approach has
// been rejected and as such we'll stick to using byte values instead.
// Note: Each byte in this array must have a corresponding tone in MarioTones[].
//
const unsigned char __attribute__ ((progmem)) MarioBeats[] =
{ 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6, 3, 3, 6, 3, 3, 3, 3, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6, 3, 3, 3, 3, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6, 3, 3,
3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6, 3, 3, 3,
3, 6, 3, 3, 6, 12, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6, 3, 3, 3, 3, 3, 3, 3, 3, 12,
12, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6, 3, 3, 6,
3, 3, 3, 3, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6, 3,
3, 3, 3, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6, 3, 3,
3, 3, 6, 3, 3, 3, 3, 3, 3, 3, 3, 6, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 6, 3, 3, 3, 3, 6,
3, 3, 3, 3, 3, 3, 3, 3, 6, 3, 3, 3, 3, 4, 4, 4, 3, 3, 3, 3, 3, 12, 12, 12, 12 };

#ifdef EMULIN
// Constants derived from array sizes to be used for sanity checks in Emuchron
const int marioTonesLen = sizeof(MarioTones);
const int marioBeatsLen = sizeof(MarioBeats);
#endif
#endif

