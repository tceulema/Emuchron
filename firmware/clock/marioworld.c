//*****************************************************************************
// Filename : 'marioworld.c'
// Title    : Animation code for MONOCHRON Marioworld clock
//*****************************************************************************

// History of original code:
// Initial work based on InvaderChron by Dataman
// By: techninja (James T) & Super-Awesome Sylvia
// Originally created for Sylvia's Super-Awesome Mini Maker Show Episode S02E03
// https://sylviashow.com/episodes/s2/e3/mini/monochron/
// https://github.com/techninja/MarioChron

#ifdef EMULIN
#include "../emulator/stub.h"
#else
#include "../util.h"
#endif
#include "../ks0108.h"
#include "../monomain.h"
#include "../glcd.h"
#include "../anim.h"
#include "marioworld.h"

// The partial 7x7 monospace font and font sprite
#define FONT7X7_WIDTH	8	// Character width including space line
#define FONT7X7_NULL	255	// Defined null value
#define FONT7X7_DASH	10	// Index of 7x7m font '-' char
#define FONT7X7_COLON	11	// Index of 7x7m font ':' char

// Ground level.
// Provides a y offset for everything that's on the ground.
#define GROUND_Y	58	// Ground position

// The plant pots.
// Provides an offset for the piranha plants and the area in which Mario walks.
#define POT_WIDTH	10	// Plant pot sprite width
#define POT_HEIGHT	9	// Plant pot sprite height
#define POT_LEFT_X	2	// Left plant pot x start
#define POT_RIGHT_X	77	// Right plant pot x start
#define POT_Y		(GROUND_Y - POT_HEIGHT) // Plant pot y position

// The plateau.
// Provides an offset for the area in which the turtle walks.
#define PLATEAU_MIN	89	// Plateau x start
#define PLATEAU_MAX	126	// Plateau x end
#define PLATEAU_Y	(GROUND_Y - 15) // Plateau y position

// The bouncing blocks
#define BLOCK_WIDTH	8	// Block sprite width
#define BLOCK_HEIGHT	10	// Block sprite height
#define BLOCK_HOUR_X	40	// Hour block x position
#define BLOCK_MIN_X	56	// Minute block x position
#define BLOCK_Y		26	// Block y position
#define BLOCK_STOP	255	// No block animation
#define BLOCK_START	0	// Start block animation
#define BLOCK_BOUNCE	3	// Block bounce height
#define BLOCK_END	(BLOCK_BOUNCE * 2) // End bounce animation
#define BLOCK_COIN	(BLOCK_END - 2)	// Bounce step to trigger coin anim

// The coin emerging from bouncing block
#define COIN_WIDTH	8	// Coin sprite width
#define COIN_HEIGHT	8	// Coin sprite height
#define COIN_Y		(BLOCK_Y - COIN_HEIGHT - 1) // Coin y position
#define COIN_STOP	(sizeof(coin) / COIN_WIDTH) // No coin animation
#define COIN_START	0	// Start coin animation
#define COIN_SCORE	11	// Coin step to trigger score update

// The date/alarm value display area
#define DA_X		1	// Date/alarm x position
#define DA_Y		9	// Date/alarm y position
#define DA_WIDTH	(FONT7X7_WIDTH * 5) // Date/alarm width
#define DA_HEIGHT	8	// Date/alarm height
#define DA_ALARM	0	// Bit offset of sprite alarm text
#define DA_DATE		8	// Bit offset of sprite date text

// Good-old Mario.
// About MARIO_MOVE and MARIO_FEET defs below:
// MARIO_MOVE determines how fast Mario moves. The lower the value the faster
// Mario moves, but a fast move will make Mario look blurry on the lcd.
// MARIO_FOOT determines how fast Mario swaps his feet, based on the x location
// of Mario. The speed for swapping is also influenced by the speed with which
// Mario moves, as determined by MARIO_MOVE. The lower the value the faster
// Mario swaps his feet, but a fast swap speed will make Mario's feet look
// blurry on the lcd.
// The following combinations of both defines should be ok:
// (MARIO_MOVE,MARIO_FEET) = (0,2) or (1,1)
#define MARIO_WIDTH	9	// Mario sprite width
#define MARIO_HEIGHT	12	// Mario sprite height
#define MARIO_MIN	(POT_LEFT_X + POT_WIDTH) // Mario x left boundary
#define MARIO_MAX	(POT_RIGHT_X - MARIO_WIDTH) // Mario x right boundary
#define MARIO_MOVE	1	// Mario move animation delay cycles
#define MARIO_FEET	1	// Mario frame swap speed
#define MARIO_GROUND	0	// Mario is not jumping to coin block
#define MARIO_BLOCK	5	// Jump step to trigger bouncing block

// The piranha plants
#define PLANT_WIDTH	8	// Plant sprite width
#define PLANT_HEIGHT	10	// Plant sprite height
#define PLANT_LEFT_X	(POT_LEFT_X + 1)  // Left plant x position
#define PLANT_RIGHT_X	(POT_RIGHT_X + 1) // Right plant x position
#define PLANT_Y		(POT_Y - PLANT_HEIGHT + 1) // Plant y position
#define PLANT_EATING	26	// Erupted plant has been chewing away
#define PLANT_STOP	(PLANT_EATING + PLANT_HEIGHT) // Indicates no plant animation
#define PLANT_START	0	// Start plant animation
#define PLANT_MOVE	1	// Plant move delay animation cycles
#define PLANT_PAUSE	13	// Min secs to wait for next animation

// The time score
#define TIME_WIDTH	(FONT7X7_WIDTH * 4) // Time sprite width
#define TIME_HEIGHT	8	// Time sprite height
#define TIME_X		96	// Time x position
#define TIME_Y		1	// Time y position
#define TIME_STOP	32	// Inidicates no scrolling time score
#define TIME_START	3	// Start score animation

// Turtle Koopa Troopa
#define TURTLE_WIDTH	9	// Turtle sprite width
#define TURTLE_HEIGHT	16	// Turtle sprite height
#define TURTLE_MIN	(PLATEAU_MIN + 1) // Turtle x boundary
#define TURTLE_MAX	(PLATEAU_MAX - TURTLE_WIDTH) // Turtle x boundary
#define TURTLE_SHELL	10	// Number of u-turns before morph into shell
#define TURTLE_JUMP	4	// Turtle jump height to morph into shell
#define TURTLE_WAIT	(TURTLE_JUMP * 2 + 66)  // Wait for turtle to reappear
#define TURTLE_STOP	(TURTLE_WAIT + TURTLE_HEIGHT) // End animation
#define TURTLE_START	0	// Start turtle animation
#define TURTLE_Y	(PLATEAU_Y - TURTLE_HEIGHT) // Turtle y position
#define TURTLE_MOVE	2	// Turtle move animation delay cycles
#define SHELL_WIDTH	11	// Turtle shell sprite width
#define SHELL_HEIGHT	8	// Turtle shell sprite height
#define SHELL_X		(turX - (SHELL_WIDTH - TURTLE_WIDTH) / 2) // Shell x pos
#define SHELL_Y		(PLATEAU_Y - SHELL_HEIGHT) // Turtle shell y position
#define SHELL_TRIGGER	(TURTLE_MIN + 5) // Turtle position to morph into shell

// The world/alarm text display area
#define WA_WIDTH	40	// World/alarm sprite width
#define WA_HEIGHT	7	// World/alarm sprite height
#define WA_X		1	// World/alarm x position
#define WA_Y		1	// World/alarm y position
#define WA_ALARM	0	// Bit offset of sprite text 'ALARM'
#define WA_WORLD	8	// Bit offset of sprite text 'WORLD'

// Monochron environment variables
extern volatile u08 mcClockNewTS;
extern volatile u08 mcClockOldTM, mcClockOldTH;
extern volatile u08 mcClockNewTM, mcClockNewTH;
extern volatile u08 mcClockOldDD, mcClockOldDM;
extern volatile u08 mcClockNewDD, mcClockNewDM;
extern volatile u08 mcClockInit;
extern volatile u08 mcAlarming, mcAlarmH, mcAlarmM;
extern volatile u08 mcAlarmSwitch;
extern volatile u08 mcUpdAlarmSwitch;
extern volatile u08 mcCycleCounter;
extern volatile u08 mcClockTimeEvent;
extern volatile u08 mcBgColor, mcFgColor;
extern volatile u08 mcU8Util1;

// Static data for date/alarm and time score data
static u08 scoreDateLeft, scoreDateRight;
static u08 scoreTimeLeft, scoreTimeRight;

// 7x7 monospace font digit characters and char separators
static const uint8_t __attribute__ ((progmem)) font7x7m[] =
{
  0x1c,0x3e,0x61,0x41,0x43,0x3e,0x1c,0x00, // 0
  0x40,0x42,0x7f,0x7f,0x40,0x40,0x00,0x00, // 1
  0x62,0x73,0x79,0x59,0x5d,0x4f,0x46,0x00, // 2
  0x20,0x61,0x49,0x4d,0x4f,0x7b,0x31,0x00, // 3
  0x18,0x1c,0x16,0x13,0x7f,0x7f,0x10,0x00, // 4
  0x27,0x67,0x45,0x45,0x45,0x7d,0x38,0x00, // 5
  0x3c,0x7e,0x4b,0x49,0x49,0x79,0x30,0x00, // 6
  0x03,0x03,0x71,0x79,0x0d,0x07,0x03,0x00, // 7
  0x36,0x7f,0x49,0x49,0x49,0x7f,0x36,0x00, // 8
  0x06,0x4f,0x49,0x49,0x69,0x3f,0x1e,0x00, // 9
  0x00,0x0c,0x0c,0x0c,0x0c,0x0c,0x00,0x00, // -
  0x00,0x00,0x36,0x36,0x00,0x00,0x00,0x00  // :
};

// Static sprites
static const uint8_t __attribute__ ((progmem)) bolt[] =      // 4x4 frame
  {0x6,0xd,0xB,0x6};
static const uint8_t __attribute__ ((progmem)) cloud[] =     // 16x8 frame
  {0x36,0x49,0x91,0x82,0x41,0x81,0x85,0x42,0x81,0x81,0x85,0x42,0x81,0x93,0x92,0x6c};
static const uint8_t __attribute__ ((progmem)) ground[] =    // 8x6 frame
  {0x0d,0x10,0x09,0x04,0x19,0x20,0x11,0x08};
static const uint16_t __attribute__ ((progmem)) plantpot[] = // 10x9 frame
  {0x000f,0x01fd,0x01ab,0x0159,0x0109,0x0109,0x0109,0x0109,0x01f9,0x000f};

// Administer block animation
static u08 blockHourY;			// Position y to animate left block
static u08 blockMinY;			// Position y to animate right block
static u08 blockAnim;			// Bouncing block animation index
static u08 blockAnimX;			// Bouncing block x position
static u08 blockFrame;			// Block animation frame to be drawn
static u08 blockUpdate = GLCD_FALSE;	// Request to update frame of blocks
static const uint16_t __attribute__ ((progmem)) block[] = // 8x10 frame
{
  0x00fc,0x0102,0x010a,0x01a6,0x01b6,0x011a,0x0102,0x00fc, // Frame 0
  0x00fc,0x01fe,0x01f6,0x015a,0x014a,0x01e6,0x01fe,0x00fc  // Frame 1 (inversed)
};

// Administer coin-from-block animation
static u08 coinAnimX;			// Position x to animate coin
static u08 coinFrame;			// Coin animation frame to be drawn
static const uint8_t __attribute__ ((progmem)) coin[] = // 8x8 frame
{
  0x00,0x00,0x00,0xe0,0xe0,0x00,0x00,0x00, // Frame 0 Coin rises from block
  0x00,0x00,0x00,0x78,0x78,0x00,0x00,0x00, // Frame 1
  0x00,0x00,0x38,0xc6,0xd6,0x38,0x00,0x00, // Frame 2
  0x00,0x38,0x44,0x82,0x92,0x44,0x38,0x00, // Frame 3
  0x3c,0x42,0x81,0x81,0xa1,0x9d,0x42,0x3c, // Frame 4 Coin fully visible
  0x00,0x3c,0x42,0x81,0xbd,0x42,0x3c,0x00, // Frame 5 Coin rotates
  0x00,0x00,0x7e,0x81,0xbd,0x7e,0x00,0x00, // Frame 6
  0x00,0x00,0x00,0xff,0xff,0x00,0x00,0x00, // Frame 7
  0x00,0x00,0x7e,0x81,0xbd,0x7e,0x00,0x00, // Frame 8
  0x00,0x3c,0x42,0x81,0xbd,0x42,0x3c,0x00, // Frame 9
  0x00,0x00,0x7e,0x81,0xbd,0x7e,0x00,0x00, // Frame 10
  0x00,0x00,0x00,0xff,0xff,0x00,0x00,0x00, // Frame 11 Coin starts to disappear
  0x00,0x00,0x00,0x55,0xaa,0x00,0x00,0x00, // Frame 12
  0x00,0x00,0x00,0x3c,0x3c,0x00,0x00,0x00, // Frame 13
  0x00,0x00,0x24,0x18,0x18,0x24,0x00,0x00, // Frame 14
  0x00,0x42,0x24,0x00,0x00,0x24,0x42,0x00, // Frame 15
  0x81,0x42,0x00,0x00,0x00,0x00,0x42,0x81, // Frame 16
  0x81,0x00,0x00,0x00,0x00,0x00,0x00,0x81, // Frame 17
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00  // Frame 18 Blank out sprite area
};

// Administer date/alarm display
static uint16_t daBuf[DA_WIDTH];	// Dynamic sprite buffer date/alarm

// Administer Mario animation
static u08 marX = MARIO_MIN;		// Current x position
static u08 marY = GROUND_Y - 12;	// Current y position
static u08 marPrevX = MARIO_MIN;	// Previous x position
static u08 marPrevY = GROUND_Y - 12;	// Previous y position
static s08 marDir = 1;			// Direction (1 = LtoR or -1 = RtoL)
static u08 marJump;			// Index in jump marArc height offsets
static u08 marJumpHour;			// Request to jump at hour block
static u08 marJumpMin;			// Request to jump at min block
static u08 marJumpCnf;			// Confirm to start Mario jump
static u08 marWait;			// Cycles to wait before Mario moves
static u08 marLastFrame = 0;		// Keep same frame while jumping
static const uint8_t __attribute__ ((progmem)) marArc[] = // Jump y positions
{ 2,6,9,11,9,6,2 };
static const uint16_t __attribute__ ((progmem)) mario[] = // 9x12 frame
{
  0x0100,0x0492,0x07d6,0x06c3,0x07cb,0x0ec6,0x0e9c,0x0918,0x0000, // <-- frame 0
  0x0900,0x0e92,0x0fd6,0x06c3,0x07cb,0x06c6,0x049c,0x0118,0x0000, // <-- frame 1
  0x0000,0x0918,0x0e9c,0x0ec6,0x07cb,0x06c3,0x07d6,0x0492,0x0100, // --> frame 0
  0x0000,0x0118,0x049c,0x06c6,0x07cb,0x06c3,0x0fd6,0x0e92,0x0900  // --> frame 1
};

// Administer piranha plant animation
static u08 plaAnimX = PLANT_RIGHT_X;    // Position x to animate plant
static u08 plaFrame = PLANT_STOP;	// Plant animation frame to be drawn
static u08 plaPause = PLANT_PAUSE;	// Wait seconds to start next anim
static u08 plaWait;			// Cycles to wait before plant moves
static const uint16_t __attribute__ ((progmem)) plaOpen[] =   // 8x10 frame
{ 0x009c,0x0136,0x0158,0x03a0,0x03a0,0x0158,0x0136,0x009c };
static const uint16_t __attribute__ ((progmem)) plaClosed[] = // 8x10 frame
{ 0x0080,0x013c,0x0142,0x03ea,0x03d6,0x0142,0x013c,0x0080 };

// Administer time score animation
static u08 timePos;			// Time score y scroll position
static uint16_t timeBuf[TIME_WIDTH];	// Dynamic sprite buffer old/new score

// Administer Koopa Troopa turtle animation
static u08 turX = TURTLE_MIN;		// Current x position
static u08 turY = TURTLE_Y;		// Current y position
static u08 turShell = 0;		// Counter to wait for shell morphing
static u08 turAnim = TURTLE_STOP;	// Turtle/shell/offscreen/reappear idx
static s08 turDir = 1;			// Direction (1 = LtoR or -1 = RtoL)
static u08 turWait;			// Cycles to wait before turtle moves
static u08 turFrame;			// Turtle frame to be drawn
static const uint16_t __attribute__ ((progmem)) turtle[] = // 9x16 frame
{
  0x0078,0x2c33,0x3fff,0x3bfe,0x9fc0,0xfe80,0xf780,0x1c00,0x0000, // <-- frame 0
  0x0078,0x8c33,0xffff,0xfbfe,0x3fc0,0x3e80,0x3780,0x1c00,0x0000, // <-- frame 1
  0x0000,0x1c00,0xf780,0xfe80,0x9fc0,0x3bfe,0x3fff,0x2c33,0x0078, // --> frame 0
  0x0000,0x1c00,0x3780,0x3e80,0x3fc0,0xfbfe,0xffff,0x8c33,0x0078  // --> frame 1
};
static const uint8_t __attribute__ ((progmem)) turtleShell[] = // 11x8 frame
{ 0x00,0x40,0x78,0xb4,0xbe,0xae,0xba,0xbe,0xac,0x78,0x40 };

// Administer WORLD/ALARM header animation
static u08 waPos;			// World/alarm y scroll position
// Subset of two vertical 7x7 monospace font characters with char separator
static const uint16_t __attribute__ ((progmem)) wa7x7m[] = // 40x16 frame
{
  0x7f7c,0x7f7e,0x3813,0x1c11,0x3813,0x7f7e,0x7f7c,0x0000, // A + W
  0x3e7f,0x7f7f,0x4140,0x4140,0x4140,0x7f40,0x3e00,0x0000, // L + O
  0x7f7c,0x7f7e,0x1113,0x3111,0x7913,0x6f7e,0x4e7c,0x0000, // A + R
  0x7f7f,0x7f7f,0x4011,0x4031,0x4079,0x406f,0x004e,0x0000, // R + L
  0x7f7f,0x7f7f,0x410e,0x411c,0x630e,0x3e7f,0x1c7f,0x0000  // M + D
};

// Local function prototypes
static void marioAlmAreaUpdate(void);
static void marioBlock(void);
static void marioBufFill(u08 left, u08 separator, u08 right, u08 yOffset,
  uint16_t *buf);
static void marioBufFillElm(u08 element, u08 yOffset, uint16_t *buf);
static void marioCoin(void);
static void marioMario(void);
static void marioPlant(void);
static void marioScore(void);
static void marioScroll(u08 x, u08 y, u08 pos, u08 width, u08 origin,
  void *buf);
static void marioTurtle(void);

//
// Function: marioAlmAreaUpdate
//
// Draw updates in the alarm areas. It supports the scrolling WORLD/ALARM
// header, the alarm time underneath the header, and requesting the coin blocks
// to blink during alarming/snoozing.
// Note: It does NOT support showing the date under the W/A header. This is
// done by marioScore().
//
static void marioAlmAreaUpdate(void)
{
  u08 newAlmDisplayState = GLCD_FALSE;
  u08 hdrPos;

  // Draw initial header (WORLD or ALARM) and fill initial alarm time in buffer
  if (mcClockInit == GLCD_TRUE)
  {
    marioBufFill(mcAlarmH, FONT7X7_COLON, mcAlarmM, DA_ALARM, daBuf);
    if (mcAlarmSwitch == ALARM_SWITCH_OFF)
    {
      marioScroll(WA_X, WA_Y, WA_WORLD, WA_WIDTH, DATA_PMEM, (void *)wa7x7m);
      waPos = WA_WORLD;
    }
    else
    {
      marioScroll(WA_X, WA_Y, WA_ALARM, WA_WIDTH, DATA_PMEM, (void *)wa7x7m);
      marioScroll(DA_X, DA_Y, DA_ALARM, DA_WIDTH, DATA_RAM, (void *)daBuf);
      waPos = WA_ALARM;
    }
  }
  else if (mcUpdAlarmSwitch == GLCD_TRUE)
  {
    // If we switched on/off the alarm update alarm time in buffer
    marioBufFill(mcAlarmH, FONT7X7_COLON, mcAlarmM, DA_ALARM, daBuf);
  }

  // Determine whether we need a (continuing) rotating header draw
  if ((mcAlarmSwitch == ALARM_SWITCH_OFF && waPos != WA_WORLD) ||
      (mcAlarmSwitch == ALARM_SWITCH_ON && waPos != WA_ALARM))
  {
    // Shift one scroll pixel every cycle. Depending on target position we
    // scroll up or down.
    waPos = (waPos + 1) % 16;
    if (waPos > WA_WORLD)
      hdrPos = 16 - waPos;
    else
      hdrPos = waPos;
    marioScroll(WA_X, WA_Y, hdrPos, WA_WIDTH, DATA_PMEM, (void *)wa7x7m);
    marioScroll(DA_X, DA_Y, hdrPos, DA_WIDTH, DATA_RAM, (void *)daBuf);
  }

  // Set alarm blinking state in case we're alarming
  if (mcAlarming == GLCD_TRUE && (mcCycleCounter & 0x08) == 8)
    newAlmDisplayState = GLCD_TRUE;

  // Make alarm area blink during alarm or cleanup after end of alarm
  if (newAlmDisplayState != mcU8Util1)
  {
    // Inverse the coin blocks
    mcU8Util1 = newAlmDisplayState;
    blockUpdate = GLCD_TRUE;
    if (mcU8Util1 == GLCD_FALSE)
      blockFrame = 0;
    else
      blockFrame = 1;
  }
}

//
// Function: marioBlock
//
// Animate block
//
static void marioBlock(void)
{
  u08 posY;

  // Trigger to animate block after Mario hitting the block
  if (marJump == MARIO_BLOCK && marWait == 0)
  {
    blockAnim = BLOCK_START;
    if (marJumpHour)
      blockAnimX = BLOCK_HOUR_X;
    else
      blockAnimX = BLOCK_MIN_X;
  }

  // Do we have a bouncing block
  if (blockAnim != BLOCK_STOP)
  {
    // Set the bouncing block y position
    if (blockAnim < BLOCK_BOUNCE)
      posY = BLOCK_Y - 1 - blockAnim;
    else
      posY = BLOCK_Y - BLOCK_END + 1 + blockAnim;
    if (blockAnimX == BLOCK_HOUR_X)
      blockHourY = posY;
    else
      blockMinY = posY;

    // Set animation for next cycle and request block update
    blockAnim++;
    blockUpdate = GLCD_TRUE;
  }

  // Update the blocks when needed
  if (blockUpdate == GLCD_TRUE)
  {
    glcdBitmap16PmFg(BLOCK_HOUR_X, blockHourY, BLOCK_WIDTH, BLOCK_HEIGHT,
      block + blockFrame * BLOCK_WIDTH);
    glcdBitmap16PmFg(BLOCK_MIN_X, blockMinY, BLOCK_WIDTH, BLOCK_HEIGHT,
      block + blockFrame * BLOCK_WIDTH);
    blockUpdate = GLCD_FALSE;
  }

  // Detect end of block bounce
  if (blockAnim == BLOCK_END)
    blockAnim = BLOCK_STOP;
}

//
// Function: marioBufFill
//
// Fill a sprite buffer with 7x7 font info
//
static void marioBufFill(u08 left, u08 separator, u08 right, u08 yOffset,
  uint16_t *buf)
{
  u08 sepSize = 0;

  // Two digits from left value
  marioBufFillElm(left / 10, yOffset, buf);
  marioBufFillElm(left % 10, yOffset, &buf[FONT7X7_WIDTH]);

  // Optional value separator
  if (separator != FONT7X7_NULL)
  {
    marioBufFillElm(separator, yOffset, &buf[FONT7X7_WIDTH * 2]);
    sepSize = FONT7X7_WIDTH;
  }

  // Two digits from right value
  marioBufFillElm(right / 10, yOffset, &buf[FONT7X7_WIDTH * 2 + sepSize]);
  marioBufFillElm(right % 10, yOffset, &buf[FONT7X7_WIDTH * 3 + sepSize]);
}

//
// Function: marioBufFillElm
//
// Fill a sprite buffer with a 7x7 font element.
// Argemuent yOffset determines whether the element is saved in the upper or
// lower 8 bit of a word.
//
static void marioBufFillElm(u08 element, u08 yOffset, uint16_t *buf)
{
  u08 i;
  u08 idx = element * FONT7X7_WIDTH;

  for (i = 0; i < FONT7X7_WIDTH; i++)
    buf[i] = ((buf[i] & ~(0x00ff << yOffset)) |
      (pgm_read_byte(font7x7m + idx + i) << yOffset));
}

//
// Function: marioCoin
//
// Animate coin frame erupting from a block
//
static void marioCoin(void)
{
  // Trigger to animate coin when block is bouncing
  if (blockAnim == BLOCK_COIN)
  {
    coinFrame = COIN_START;
    coinAnimX = blockAnimX;
  }

  // If there's no coin animation we're done
  if (coinFrame == COIN_STOP)
    return;

  // Draw frame and set next frame (that may indicate animation stop)
  glcdBitmap8PmFg(coinAnimX, COIN_Y, COIN_WIDTH, COIN_HEIGHT,
    coin + coinFrame * COIN_WIDTH);
  coinFrame++;
}

//
// Function: marioCycle
//
// Update the lcd display of a marioworld clock
//
void marioCycle(void)
{
  marioAlmAreaUpdate();
  marioMario();
  marioBlock();
  marioCoin();
  marioTurtle();
  marioPlant();
  marioScore();
}

//
// Function: marioInit
//
// Initialize the lcd display of a marioworld clock
//
void marioInit(u08 mode)
{
  u08 i;

  DEBUGP("Init Mario");

  // Score coin and 'x' at top-right
  glcdBitmap8PmFg(80, 1, COIN_WIDTH, COIN_HEIGHT, coin + 4 * COIN_WIDTH);
  glcdPutStr2(90, 1, FONT_5X7M, "x", mcFgColor);

  // Draw the bolted plateau for the turtle
  glcdRectangle(PLATEAU_MIN, PLATEAU_Y, PLATEAU_MAX - PLATEAU_MIN + 1,
    GROUND_Y - PLATEAU_Y + 1, mcFgColor);
  glcdDot(PLATEAU_MIN, PLATEAU_Y, mcBgColor);
  glcdDot(PLATEAU_MAX, PLATEAU_Y, mcBgColor);
  glcdBitmap8PmFg(PLATEAU_MIN + 2, PLATEAU_Y + 2, 4, 4, bolt);
  glcdBitmap8PmFg(PLATEAU_MAX - 5, PLATEAU_Y + 2, 4, 4, bolt);

  // Other images (ground, clouds, plant pots and coin blocks)
  for (i = 0; i < 16; i++)
    glcdBitmap8PmFg(i * 8, GROUND_Y, 8, 6, ground);
  glcdBitmap8PmFg(54, 5, 16, 8, cloud);
  glcdBitmap8PmFg(12, 21, 16, 8, cloud);
  glcdBitmap8PmFg(100, 12, 16, 8, cloud);
  glcdBitmap16PmFg(POT_LEFT_X, POT_Y, POT_WIDTH, POT_HEIGHT, plantpot);
  glcdBitmap16PmFg(POT_RIGHT_X, POT_Y, POT_WIDTH, POT_HEIGHT, plantpot);
  glcdBitmap16PmFg(BLOCK_HOUR_X, BLOCK_Y, 8, 10, block);
  glcdBitmap16PmFg(BLOCK_MIN_X, BLOCK_Y, 8, 10, block);

  // Init some data to prevent graphic anomalies at (re)start: the blocks don't
  // bounce, the coin does not animate and mario does not jump. Also, mario,
  // the turtle and (when active) piranha plant are set to move immediately.
  blockFrame = 0;
  blockHourY = BLOCK_Y;
  blockMinY = BLOCK_Y;
  blockAnim = BLOCK_STOP;
  coinFrame = COIN_STOP;
  marJump = MARIO_GROUND;
  marJumpCnf = GLCD_FALSE;
  marJumpHour = GLCD_FALSE;
  marJumpMin = GLCD_FALSE;
  marWait = MARIO_MOVE;
  plaWait = PLANT_MOVE;
  turWait = TURTLE_MOVE;
  mcU8Util1 = GLCD_FALSE;
}

//
// Function: marioMario
//
// Animate running/turning/jumping mario
//
static void marioMario(void)
{
  u08 frame;

  // If the minute or hour has changed and no coin is animated, jump the next
  // chance we get
  if (mcClockTimeEvent == GLCD_TRUE && marJumpHour == GLCD_FALSE &&
      marJumpMin == GLCD_FALSE && coinFrame == COIN_STOP)
  {
    // Hour change has precedence over minute change
    if (scoreTimeLeft != mcClockNewTH)
      marJumpHour = GLCD_TRUE;
    else if (scoreTimeRight != mcClockNewTM)
      marJumpMin = GLCD_TRUE;
  }

  // Mario waits some clock cycles before he moves
  if (marWait < MARIO_MOVE)
  {
    marWait++;
    return;
  }
  marWait = 0;

  // On specific points we may start jumping
  if ((marJumpHour && marX == BLOCK_HOUR_X - marDir * 4) ||
      (marJumpMin && marX == BLOCK_MIN_X - marDir * 4))
    marJumpCnf = GLCD_TRUE;

  // Set default y and modify in case we are jumping
  marY = GROUND_Y - MARIO_HEIGHT;
  if (marJumpCnf)
  {
    // Overide y due to jumping
    marY = marY - pgm_read_byte(marArc + marJump);

    // Set next jump step but stop when last jump step was reached
    marJump++;
    if (marJump == sizeof(marArc))
    {
      marJumpHour = GLCD_FALSE;
      marJumpMin = GLCD_FALSE;
      marJumpCnf = GLCD_FALSE;
      marJump = MARIO_GROUND;
    }
  }

  // Clear previous frame if we are jumping and save new location
  if (marPrevY != marY)
    glcdFillRectangle(marPrevX, marPrevY, MARIO_WIDTH, MARIO_HEIGHT,
      mcBgColor);
  marPrevX = marX;
  marPrevY = marY;

  // Determine frame to draw and draw it. When Mario is jumping don't swap
  // his feet during the jumping process.
  if (marJumpCnf == GLCD_TRUE)
    frame = marLastFrame;
  else
    frame = marDir + 1 + ((marX >> MARIO_FEET) & 0x1);
  marLastFrame = frame;
  glcdBitmap16PmFg(marX, marY, MARIO_WIDTH, MARIO_HEIGHT,
    mario + frame * MARIO_WIDTH);

  // Move for next draw and switch direction when needed
  marX = marX + marDir;
  if (marX == MARIO_MIN || marX == MARIO_MAX)
    marDir = -marDir;
}

//
// Function: marioPlant
//
// Animate hungry piranha plant frame erupting from or hiding into a plant pot
//
static void marioPlant(void)
{
  u08 height = 1;

  // See if we need to start animating the plant
  if (plaFrame == PLANT_STOP)
  {
    if (mcClockTimeEvent == GLCD_FALSE)
      return;

    // Countdown for next plant to animate
    if (plaPause > 0)
    {
      plaPause--;
      return;
    }

    // Swap plant pot and initiate animation
    if (plaAnimX == PLANT_LEFT_X)
      plaAnimX = PLANT_RIGHT_X;
    else
      plaAnimX = PLANT_LEFT_X;
    plaFrame = PLANT_START;
    plaPause = PLANT_PAUSE;
    plaWait = PLANT_MOVE;
  }

  // Plant waits some clock cycles before it moves
  if (plaWait < PLANT_MOVE)
  {
    plaWait++;
    return;
  }
  plaWait = 0;

  // Determine how much to shift frame data to mimick erupting/disapparing
  // plant from pot, and set next plant frame
  if (plaFrame < PLANT_HEIGHT)
    height = plaFrame;
  else if (plaFrame < PLANT_EATING)
    height = PLANT_HEIGHT;
  else if (plaFrame < PLANT_STOP)
    height = PLANT_STOP - plaFrame;
  plaFrame++;

  // Draw (a height subset of) the open or closed plant frame
  if ((plaFrame & 0x2) == 0)
    glcdBitmap16PmFg(plaAnimX, POT_Y - height, PLANT_WIDTH, height,
      plaOpen);
  else
    glcdBitmap16PmFg(plaAnimX, POT_Y - height, PLANT_WIDTH, height,
      plaClosed);
}

//
// Function: marioScore
//
// Write current time and, when appropriate, the current date
//
static void marioScore(void)
{
  if (mcClockInit == GLCD_TRUE)
  {
    // Administer static scores independent from actual time/date
    scoreTimeLeft = mcClockNewTH;
    scoreTimeRight = mcClockNewTM;
    scoreDateLeft = mcClockNewDD;
    scoreDateRight = mcClockNewDM;

    // Draw time
    marioBufFill(mcClockNewTH, FONT7X7_NULL, mcClockNewTM, 0, timeBuf);
    marioScroll(TIME_X, TIME_Y, 0, TIME_WIDTH, DATA_RAM, (void *)timeBuf);
    timePos = TIME_STOP;

    // Fill date and draw when alarm switch is off
    marioBufFill(mcClockNewDD, FONT7X7_DASH, mcClockNewDM, DA_DATE, daBuf);
    if (mcAlarmSwitch == ALARM_SWITCH_OFF)
      marioScroll(DA_X, DA_Y, DA_DATE, DA_WIDTH, DATA_RAM, (void *)daBuf);
  }

  // Do we need to update time and date
  if (coinFrame == COIN_SCORE)
  {
    // New time score so initiate to scroll
    timePos = TIME_START;
    marioBufFill(scoreTimeLeft, FONT7X7_NULL, scoreTimeRight, 0, timeBuf);
    marioBufFill(mcClockNewTH, FONT7X7_NULL, mcClockNewTM, 8, timeBuf);
    scoreTimeLeft = mcClockNewTH;
    scoreTimeRight = mcClockNewTM;

    // Update date info and draw only when we're at static date position
    marioBufFill(mcClockNewDD, FONT7X7_DASH, mcClockNewDM, DA_DATE, daBuf);
    if (waPos == WA_WORLD)
      marioScroll(DA_X, DA_Y, DA_DATE, DA_WIDTH, DATA_RAM, (void *)daBuf);
    scoreDateLeft = mcClockNewDD;
    scoreDateRight = mcClockNewDM;
  }

  // Determine whether we need a (continuing) scrolling time score draw
  if (timePos != TIME_STOP)
  {
    // Shift one scroll pixel every four cycles
    timePos = timePos + 1;
    if ((timePos & 0x3) == 0)
      marioScroll(TIME_X, TIME_Y, timePos / 4, TIME_WIDTH, DATA_RAM,
        (void * *)timeBuf);
  }
}

//
// Function: marioScroll
//
// Animate step in vertically scrolling between two bitmap 7x7 text images
//
static void marioScroll(u08 x, u08 y, u08 pos, u08 width, u08 origin,
  void *buf)
{
  glcdBitmap(x, y, 0, pos, width, 7, ELM_WORD, origin, buf, mcFgColor);
}

//
// Function: marioTurtle
//
// Animate running/turning turtle
//
static void marioTurtle(void)
{
  u08 drawWidth = SHELL_WIDTH;

  // Turtle waits some clock cycles before it moves
  if (turWait < TURTLE_MOVE)
  {
    turWait++;
    return;
  }

  // Reset wait cycle and move turtle
  if (turDir == 1 && turX == SHELL_TRIGGER && turShell >= TURTLE_SHELL)
  {
    turX--;
    turAnim = TURTLE_START;
    turWait = TURTLE_MOVE;
    turShell = 0;
  }
  else if (turAnim == TURTLE_STOP)
  {
    // Regular move from left to right and back
    turWait = 0;
    turFrame = turDir + 1 + (turX & 0x1);
    glcdBitmap16PmFg(turX, turY, TURTLE_WIDTH, TURTLE_HEIGHT,
      turtle + turFrame * TURTLE_WIDTH);

    // Move for next draw and switch direction when needed
    turX = turX + turDir;
    if (turX == TURTLE_MIN || turX == TURTLE_MAX)
    {
      turDir = -turDir;
      turShell++;
    }
    return;
  }

  // Do special animation of the turtle
  if (turAnim < TURTLE_JUMP)
  {
    // Ascend turtle 4px high
    glcdBitmap16PmFg(turX, turY - turAnim - 1, TURTLE_WIDTH, TURTLE_HEIGHT,
      turtle + turFrame * TURTLE_WIDTH);
    glcdLine(turX, PLATEAU_Y - 1 - turAnim, turX + TURTLE_WIDTH,
      PLATEAU_Y - 1 - turAnim, mcBgColor);
  }
  else if (turAnim < TURTLE_JUMP * 2 + 1)
  {
    // Change into turtle shell that descends back to the plateau
    if (turAnim == TURTLE_JUMP)
      glcdFillRectangle(turX, turY - TURTLE_JUMP, TURTLE_WIDTH, TURTLE_HEIGHT,
        mcBgColor);
    glcdBitmap8PmFg(SHELL_X, SHELL_Y - (TURTLE_JUMP * 2 - turAnim),
      SHELL_WIDTH, SHELL_HEIGHT, turtleShell);
  }
  else if (turAnim < TURTLE_JUMP * 2 + 35)
  {
    // Shell slides off the display
    if (turAnim >= TURTLE_JUMP * 2 + 25)
      drawWidth = SHELL_WIDTH - turAnim + TURTLE_JUMP * 2 + 24;
    glcdBitmap8PmFg(SHELL_X + turAnim - TURTLE_JUMP * 2, SHELL_Y, drawWidth,
      SHELL_HEIGHT, turtleShell);
  }
  else if (turAnim < TURTLE_WAIT)
  {
    // Wait some time
    if (turAnim == TURTLE_WAIT - 1)
      turX = TURTLE_MIN;
  }
  else if (turAnim < TURTLE_WAIT + TURTLE_HEIGHT)
  {
    // Make turtle re-appear at start position
    turFrame = turDir + 1 + (turX & 0x1);
    glcdBitmap16PmFg(turX, PLATEAU_Y - (turAnim - TURTLE_WAIT + 1),
      TURTLE_WIDTH, turAnim - TURTLE_WAIT + 1,
      turtle + turFrame * TURTLE_WIDTH);
  }

  // Set next animation step
  turAnim++;
}
