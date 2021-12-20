//*****************************************************************************
// Filename : 'avrlibtypes.h'
// Title    : AVRlib global types and typedefines include file
//*****************************************************************************

#ifndef AVRLIBTYPES_H
#define AVRLIBTYPES_H

// Datatype definitions.
// There is a difference between the size of 32-bit and 64-bit Atmel typedefs,
// and corresponding Linux 64-bit OS typedefs. Therefor, separate sections are
// defined that result in identical typedef sizes between the two environments.
// Whenever different typedef sizes occur, the Atmel size is leading, being our
// target runtime environment.

#ifndef EMULIN
// Monchron Atmel defs
#include <avr/io.h>
typedef unsigned char  u08;
typedef   signed char  s08;
typedef unsigned short u16;
typedef   signed short s16;
typedef unsigned long  u32;
typedef   signed long  s32;
typedef unsigned long long u64;
typedef   signed long long s64;

typedef unsigned char  BOOL;
typedef unsigned char  BYTE;
typedef unsigned int   WORD;
typedef unsigned long  DWORD;
typedef unsigned char  UCHAR;
typedef unsigned int   UINT;
typedef unsigned short USHORT;
typedef unsigned long  ULONG;
typedef char           CHAR;
typedef int            INT;
typedef long           LONG;
#else
// Emuchron Linux 64-bit OS defs
#include <stdint.h>
typedef __uint8_t  u08;
typedef __int8_t   s08;
typedef __uint16_t u16;
typedef __int16_t  s16;
typedef __uint32_t u32;
typedef __int32_t  s32;
typedef __uint64_t u64;
typedef __int64_t  s64;
typedef __uint8_t  uint8_t;
typedef __int8_t   int8_t;
typedef __uint16_t uint16_t;
typedef __int16_t  int16_t;
typedef __uint32_t uint32_t;
typedef __int32_t  int32_t;
typedef __uint64_t uint64_t;
typedef __int64_t  int64_t;

typedef __uint8_t  BOOL;
typedef __uint8_t  BYTE;
typedef __uint16_t WORD;
typedef __uint32_t DWORD;
typedef __uint8_t  UCHAR;
typedef __uint16_t UINT;
typedef __uint16_t USHORT;
typedef __uint32_t ULONG;
typedef __int8_t   CHAR;
typedef __int16_t  INT;
typedef __int32_t  LONG;
#endif

// Maximum value that can be held by unsigned data types (8/16/32 bits)
#define MAX_U08		255
#define MAX_U16		65535
#define MAX_U32		4294967295
#define MAX_UINT8_T	255
#define MAX_UINT16_T	65535
#define MAX_UINT32_T	4294967295

// Min/Max values that can be held by signed data types (8/16/32 bits)
#define MIN_S08		-128
#define MAX_S08		127
#define MIN_S16		-32768
#define MAX_S16		32767
#define MIN_S32		-2147483648
#define MAX_S32		2147483647
#define MIN_INT8_T	-128
#define MAX_INT8_T	127
#define MIN_INT16_T	-32768
#define MAX_INT16_T	32767
#define MIN_INT32_T	-2147483648
#define MAX_INT32_T	2147483647
#endif
