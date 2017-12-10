//*****************************************************************************
// Filename : 'mariotune.h'
// Title    : Raw piezo data for playing the world famous Mario melody
//*****************************************************************************

// The tone and beat data is based on the following cute Arduino project:
// http://www.youtube.com/watch?v=VqeYvJpibLY

#ifndef MARIOTUNE_H
#define MARIOTUNE_H

#include "alarm.h"

#ifdef MARIO
//
// The Arduino firmware stores the 295 notes as a char array and maps each note
// to a tone via a linear search algorithm in two other arrays. This is
// considered too CPU intensive as our code runs in a 1 msec interrupt handler.
// So, we are going to store the notes as tones. Two things need to be taken
// care of.
// First, the Monochron piezo is not able to properly reproduce low frequency
// tones. We have to add one octave to the original tones to make the audio
// audible anyway.
// Second, we need tones over the 255 Hz range, and to be able to store these
// values we need 16 bit integers. To reduce the data space footprint we want
// to put a tone in a single byte.
// To meet both challenges, a Mario note from the Arduino firmware is converted
// into a base tone and is increased 1 octave. To allow storing the resulting
// tone in a single byte it is divided by MAR_TONEFACTOR. The densed tone value
// is rounded upwards or downwards to minimize the delta upon reproducing its
// value. This means that a reproduced tone (= tone * MAR_TONEFACTOR) is off by
// at most MAR_TONEFACTOR / 2 = +/-4 Hz.
// Since the quality of the piezo speaker in Monochron is worse than miserable,
// this 4 Hz delta sounds (pun intended) acceptable to me.
//
// Then there's the following. The Mario tune partly consists of repeated
// sequences of tones+durations. By cutting out the redundancy we can reduce
// the size of the tones+durations arrays. So, instead of 295 bytes for the
// entire tune we can cut it back to 10 unique tone+duration sequences
// requiring only 150 bytes. By applying this approach on both the tone and
// durations arrays below we'll save a total of 290 data bytes. A nice side
// effect of this is that because the array size is now less than 255 elements
// we now can use uint8_t array indices instead of uint16_t indices, leading t
// a substantial save in generated object code. The downside of this is that we
// need a master table that defines the sequence of unique tones+durations sets
// to be played, and that the logic to play these sets becomes slightly more
// complicated. But, these extra costs are outweighed by the savings reached by
// no longer needing the linear search arrays in the firmware.
// All in all, by cutting out the data redundancy, we save an additional ~225
// bytes on the image size, free to be used by clock code.
//
// Note: Each byte in MarioTones[] must have a corresponding byte in
//       MarioBeats[].
//
const unsigned char __attribute__ ((progmem)) MarioTones[] =
{
// Metadata: playLine + byteIdx + byteLen + byteLenTotal
/*  1   0 14  14 */ 146, 146, 0, 146, 0, 116, 146, 0, 174, 0, 0, 87, 0, 0,
/*  2  14 29  43 */ 116, 0, 0, 87, 0, 73, 0, 0, 98, 0, 110, 0, 104, 98, 0, 87, 146,
                    174, 196, 0, 155, 174, 0, 146, 0, 116, 130, 110, 0,
/*  3  43 15  58 */ 0, 174, 164, 155, 138, 0, 146, 0, 92, 98, 116, 0, 98, 116, 130,
/*  4  58 14  72 */ 0, 174, 164, 155, 138, 0, 146, 0, 232, 0, 232, 232, 0, 0,
/*  5  72 10  82 */ 0, 138, 0, 0, 130, 0, 116, 0, 0, 0,
/*  6  82 15  97 */ 116, 116, 0, 116, 0, 116, 130, 0, 146, 116, 0, 98, 87, 0, 0,
/*  7  97 10 107 */ 116, 116, 0, 116, 0, 116, 130, 146, 0, 0,
/*  8 107 14 121 */ 146, 116, 0, 87, 0, 92, 0, 98, 155, 0, 155, 98, 0, 0,
/*  9 121 13 134 */ 110, 196, 196, 196, 174, 155, 146, 116, 0, 98, 87, 0, 0,
/* 10 134 16 150 */ 110, 155, 0, 155, 155, 146, 130, 116, 73, 0, 73, 58, 0, 0, 0, 0
};

//
// The Arduino firmware stores the 295 beats as floats, which is 4 bytes per
// beat. To reduce the program space footprint and to avoid the use of floating
// point arithmetic we want put a single beat in a byte. A densed Mario beat is
// put in a byte using division factor MAR_BEATFACTOR. This approach saves us
// CPU expensive floating point arithmetic and lots of data space. And, as
// noted above, by cutting out data redundancy as well, instead of 1180 data
// bytes (295 floats) we'll only need 150. Nice!
// Concerning inaccuracies in reproducing durations, here's the worst-case
// scenario: When in the Arduino data a beat duration 1.3 is used this leads to
// a duration of 1.3 * MAR_TEMPO = 123 msec. Our implementation for the same
// beat in a byte will lead to a duration of 4 * MAR_TEMPO / MAR_BEATFACTOR =
// 126 msec. Not bad.
// Since the quality of the piezo speaker in Monochron is worse than miserable
// this 3 msec delta sounds (pun intended) acceptable to me.
//
const unsigned char __attribute__ ((progmem)) MarioBeats[] =
{
// Metadata: playLine + byteIdx + byteLen + byteLenTotal
/*  1   0 14  14 */ 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6, 3, 3, 6,
/*  2  14 29  43 */ 3, 3, 3, 3, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
                    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6,
/*  3  43 15  58 */ 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
/*  4  58 14  72 */ 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6,
/*  5  72 10  82 */ 6, 3, 3, 3, 3, 6, 3, 3, 6, 12,
/*  6  82 15  97 */ 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6,
/*  7  97 10 107 */ 3, 3, 3, 3, 3, 3, 3, 3, 12, 12,
/*  8 107 14 121 */ 3, 3, 3, 3, 6, 3, 3, 3, 3, 3, 3, 3, 3, 6,
/*  9 121 13 134 */ 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 6,
/* 10 134 16 150 */ 3, 3, 3, 3, 4, 4, 4, 3, 3, 3, 3, 3, 12, 12, 12, 12
};

//
// The Mario tune partly consists of repeated sets of tones with play duration.
// In total 295 tones are played. Data is saved by removing the redundancy and
// instead create data for only uniquely played parts. This will save us ~290
// tone and duration bytes. However, in order to play Mario properly we will
// now need a master table to play the sequence of unique tune parts. It turns
// out that Mario consists of 17 lines. Array MarioMaster[] introduces this
// master play table, at a cost of 34 bytes.
// Each play line consists of two bytes:
// - The first byte is the index in the tones+duration arrays for the line to
//   play.
// - The second byte is the number of tones to play for the line (after which
//   to move on to play the next line or restart at the beginning).
//
const unsigned char __attribute__ ((progmem)) MarioMaster[] =
{
// Metadata: line + playLine + byteIdx + byteLen + byteLenTotal
/*  1  1   0 14  14 */   0, 14,
/*  2  2  14 29  43 */  14, 29,
/*  3  2  14 29  72 */  14, 29,
/*  4  3  43 15  87 */  43, 15,
/*  5  4  58 14 101 */  58, 14,
/*  6  3  43 15 116 */  43, 15,
/*  7  5  72 10 126 */  72, 10,
/*  8  6  82 15 141 */  82, 15,
/*  9  7  97 10 151 */  97, 10,
/* 10  6  82 15 166 */  82, 15,
/* 11  1   0 14 180 */   0, 14,
/* 12  2  14 29 209 */  14, 29,
/* 13  2  14 29 238 */  14, 29,
/* 14  8 107 14 252 */ 107, 14,
/* 15  9 121 13 265 */ 121, 13,
/* 16  8 107 14 279 */ 107, 14,
/* 17 10 134 16 295 */ 134, 16
};

#ifdef EMULIN
// Constants derived from array sizes to be used for sanity checks in Emuchron
const int marioTonesLen = sizeof(MarioTones);
const int marioBeatsLen = sizeof(MarioBeats);
const int marioMasterLen = sizeof(MarioMaster);
#endif
#endif
#endif
