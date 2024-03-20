//*****************************************************************************
// Filename  : 'pong.c'
// Title     : Animation code for MONOCHRON pong clock
//*****************************************************************************

#include <math.h>
#include "../global.h"
#include "../glcd.h"
#include "../anim.h"
#include "../ks0108.h"
#include "../ks0108conf.h"
#include "../monomain.h"
#include "pong.h"

// The TRAJ_LEN is influenced by both the ball speed and the minimum angle.
// In short, when decreasing the ball speed or decreasing the minimum angle
// the number of ball trajectory steps will increase.
// The ball speed is a trade off between ball sluggishness and the ball moving
// too fast. Its value is in vector arithmatic.
// The min angle reduces the random angle degree range to 0+angle..180-angle,
// preventing too steep ball motion angles. The steeper the angle, the more
// trajectory steps are needed.
// Emperically, with the speed set to 5 and the min angle set to 40, a max
// total of 36 trajectory steps are needed. However in the define below two
// additional steps are added to be *really* sure we'll never overflow.
#define BALL_SPEED_MAX		5
#define BALL_ANGLE_MIN		40
#define BALL_WIDLEN		2	// Square ball width and length
#define TRAJ_LEN		38

// Create a new ball motion angle
#define ANGLE_NEW		255

// Pixel height of the top+bottom bars and pixel width of dashed middle line
#define BAR_H			2
#define MIDLINE_W		1

// Location and size of game score digits (usually showing time hour+minute)
#define SCORE_TIME_Y		(BAR_H + 2)
#define SCORE_H10_X		34
#define SCORE_H1_X		49
#define SCORE_M10_X		70
#define SCORE_M1_X		85
#define SCORE_DIGIT_H		18
#define SCORE_DIGIT_W		8

// Score mode for time (default), date/year/alarm (few secs when appropriate
// button is pressed) or the alarm (few secs when alarm is switched on)
#define SCORE_MODE_INIT		0
#define SCORE_MODE_TIME		1
#define SCORE_MODE_DATE		2
#define SCORE_MODE_YEAR		3
#define SCORE_MODE_ALARM	4

// Paddle x locations, size and max autoplay speed
#define PADDLE_LEFT_X		10
#define PADDLE_RIGHT_X		(GLCD_XPIXELS - PADDLE_W - 10)
#define PADDLE_H		12
#define PADDLE_W		3
#define PADDLE_SPEED_MAX	5

// Ball destination paddle (do *NOT* change these values)
#define PADDLE_LEFT		-1
#define PADDLE_RIGHT		1

// Ball trajectory paddle collision status
#define COLL_NONE		0
#define COLL_REQ		1
#define COLL_CONF		2

// Time in app cycles (representing ~3 secs) for non-time info to be displayed
#define COUNTDOWN_SCORE		(u08)(1000L * 3 / ANIM_TICK_CYCLE_MS)

// Time in app cycles (representing ~2 sec) to pauze before starting a new game
#define COUNTDOWN_GAME		(u08)(1000L * 2 / ANIM_TICK_CYCLE_MS)

// Uncomment this if you want to keep the ball always in the vertical centre
// of the display. Give it a try and you'll see what I mean...
//#define BALL_VCENTERED

// Monochron environment variables
extern volatile uint8_t mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcAlarming, mcAlarmH, mcAlarmM;
extern volatile uint8_t mcAlarmSwitch;
extern volatile uint8_t mcAlarmSwitchEvent;
extern volatile uint8_t mcCycleCounter;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcFgColor;
// mcU8Util1 = flashing state of the paddles during alarm/snooze
// mcU8Util2 = new score mode
// mcU8Util3 = current score mode
// mcU8Util4 = score display timeout timer
extern volatile uint8_t mcU8Util1, mcU8Util2, mcU8Util3, mcU8Util4;

// The big digit font character set describes seven segment bits per digit.
// Per bit segment in a font byte (MSB..LSB):
// 0-midbottom-midcenter-midtop-rightbottom-righttop-leftbottom-lefttop
static const unsigned char __attribute__ ((progmem)) bigFont[] =
{
  // 0     1     2     3     4     5     6     7     8     9
  0x5f, 0x0c, 0x76, 0x7c, 0x2d, 0x79, 0x7b, 0x1c, 0x7f, 0x7d
};

// The stored ball trajectory and its metadata
static u08 trajX[TRAJ_LEN];	// The ball trajectory x positions
static u08 trajY[TRAJ_LEN];	// The ball trajectory y positions
static u08 ticksPlay;		// Play ticks of complete trajectory
static u08 tickNow;		// Current trajectory position being played
static s08 paddle;		// Target trajectory paddle (left or right)
static s08 paddleY;		// Target y position of paddle in trajectory
static u08 paddleTick;		// Play tick for the ball to reach the paddle
static s08 ballDirX;		// Next trajectory: ball moving left or right
static s08 ballDirY;		// Next trajectory: ball moving up or down
static u08 ballAngle;		// Next trajectory: ball angle retain or new

// Left and right paddle information
static s08 rightPaddleY, leftPaddleY;
static s08 oldLeftPaddleY, oldRightPaddleY;
static u08 almDisplayState;

// Score, time and play pause
static u08 scoreLeft, scoreRight;
static u08 scoreRedraw;
static u08 minuteChanged, hourChanged;
static u08 countdown;

// Random value for determining the direction angle of the ball
static u16 pongRandBase = M_PI * M_PI * 1000;
static const float pongRandSeed = 3.9147258617;
static u16 pongRandVal = 0x5a3c;

// Local function prototypes
static u08 pongBallIntersect(u08 x1, u08 y1, u08 x2, u08 y2, u08 w2, u08 h2);
static void pongBallTraject(void);
static void pongBallVector(float *ballDx, float *ballDy);
static float pongBarBounce(float y, float *ballDy);
static void pongDrawBall(u08 remove);
static void pongDrawBigDigit(u08 x, u08 value, u08 high);
static void pongDrawPaddle(s08 x, s08 oldY, s08 newY);
static void pongDrawScore(void);
static void pongDrawMidLine(void);
static void pongGameStep(void);
static s08 pongPaddleMove(s08 y);
static u16 pongRandGet(u08 type);

//
// Function: pongButton
//
// Process pressed button for the pong clock
//
void pongButton(u08 pressedButton)
{
  // Set score to date (then year, (optionally) alarm and back to time)
  DEBUG(putstring_nl("BTN  press"); putstring_nl("SCOR date"));
  mcU8Util2 = SCORE_MODE_DATE;
  mcU8Util4 = COUNTDOWN_SCORE + 1;
}

//
// Function: pongCycle
//
// Update the lcd display of a pong clock
//
void pongCycle(void)
{
  // Signal a change in minutes or hours if not signalled earlier
  if (mcClockTimeEvent == MC_TRUE && minuteChanged == MC_FALSE &&
      hourChanged == MC_FALSE)
  {
    // A change in hours has priority over change in minutes
    if (mcClockOldTH != mcClockNewTH)
    {
      DEBUGP("TIME hour");
      hourChanged = MC_TRUE;
    }
    else if (mcClockOldTM != mcClockNewTM)
    {
      DEBUGP("TIME min");
      minuteChanged = MC_TRUE;
    }
  }
  //DEBUG(uart_put_dec(mcClockTimeEvent); putstring(", "));
  //DEBUG(uart_put_dec(minuteChanged); putstring(", "));
  //DEBUG(uart_put_dec(hourChanged); putstring_nl(""));

  // Set the flashing state of the paddles in case of alarming
  if (mcAlarming == MC_TRUE && (mcCycleCounter & 0x08) == 8)
    almDisplayState = MC_TRUE;
  else
    almDisplayState = MC_FALSE;

  // Do we need to change the score
  if (mcAlarmSwitchEvent == MC_TRUE)
  {
    if (mcAlarmSwitch == ALARM_SWITCH_ON)
    {
      // We're switched on so we may have to show the alarm time. Only do so if
      // we're not initializing pong.
      if (mcClockInit == MC_FALSE)
      {
        DEBUGP("SCOR alarm");
        mcU8Util2 = SCORE_MODE_ALARM;
        mcU8Util4 = COUNTDOWN_SCORE;
      }
    }
    else
    {
      // We're switched off. Revert back to time mode only when showing alarm.
      if (mcU8Util2 == SCORE_MODE_ALARM)
      {
        DEBUGP("SCOR time");
        mcU8Util2 = SCORE_MODE_TIME;
        mcU8Util4 = 0;
      }
    }
  }

  // Determine the next pong gameplay step
  pongGameStep();

  // Draw the ball
  if (countdown == 0)
  {
    // Regular game play so the ball moved from a to b. Draw ball and redraw
    // middle line when intersected by the old ball.
    pongDrawBall(MC_TRUE);
    if (pongBallIntersect(trajX[tickNow - 1], trajY[tickNow - 1],
        GLCD_XPIXELS / 2 - MIDLINE_W, 0, MIDLINE_W, GLCD_YPIXELS))
      pongDrawMidLine();
  }
  else
  {
    countdown--;
  }

#ifdef BALL_VCENTERED
  // Keep the ball always in the vertical centre of the display. This means
  // that the entire play field will shift vertically up or down to keep the
  // ball vertically centered. For this we'll use the lcd controller
  // 'startline' register that will do the vertical shift of lcd image data in
  // hardware.
  // The BALL_VCENTERED functionality in this clock is rather useless apart
  // from the fact that it nicely demonstrates what the lcd controller
  // startline register actually does :-)
  u08 i;
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    glcdControlWrite(i, GLCD_START_LINE |
      (trajY[tickNow] + GLCD_YPIXELS / 2 + BALL_WIDLEN) % GLCD_YPIXELS);
#endif

  // Move the paddles and save paddle alarm state for next cycle
  pongDrawPaddle(PADDLE_LEFT_X, oldLeftPaddleY, leftPaddleY);
  pongDrawPaddle(PADDLE_RIGHT_X, oldRightPaddleY, rightPaddleY);
  mcU8Util1 = almDisplayState;

  // Draw score and redraw ball in case it got removed by the score draw
  pongDrawScore();
  pongDrawBall(MC_FALSE);
}

//
// Function: pongInit
//
// Initialize the lcd display of a pong clock
//
void pongInit(u08 mode)
{
  // Draw top+bottom bar and dotted vertical line in middle
  glcdFillRectangle(0, 0, GLCD_XPIXELS, BAR_H);
  glcdFillRectangle(0, GLCD_YPIXELS - BAR_H, GLCD_XPIXELS, BAR_H);
  pongDrawMidLine();

  // Init pong score and paddle positions
  mcU8Util2 = SCORE_MODE_TIME;
  mcU8Util3 = SCORE_MODE_INIT;
  mcU8Util4 = 0;
  minuteChanged = hourChanged = MC_FALSE;
  oldLeftPaddleY = oldRightPaddleY = leftPaddleY = rightPaddleY = 25;

  // Init calculating first ball trajectory
  ticksPlay = tickNow = 0;
  paddleTick = 1;
  ballDirX = pongRandGet(2);
  if (ballDirX == 0)
    ballDirX = -1;
  ballDirY = pongRandGet(2);
  if (ballDirY == 0)
    ballDirY = -1;
  ballAngle = ANGLE_NEW;
  trajX[0] = GLCD_XPIXELS / 2 - BALL_WIDLEN;
  trajY[0] = GLCD_YPIXELS / 2 - BALL_WIDLEN;
}

//
// Function: pongBallIntersect
//
// Determine whether the ball rectangle overlaps with another rectangle.
// If so, return MC_TRUE.
//
static u08 pongBallIntersect(u08 x1, u08 y1, u08 x2, u08 y2, u08 w2, u08 h2)
{
  if (x1 + BALL_WIDLEN * 2 <= x2 || x2 + w2 <= x1 ||
      y1 + BALL_WIDLEN * 2 <= y2 || y2 + h2 <= y1)
    return MC_FALSE;
  //DEBUG(putstring("INTS pos=["); uart_put_dec(x1); putstring(","));
  //DEBUG(uart_put_dec(y1); putstring_nl("]"));
  return MC_TRUE;
}

//
// Function: pongBallTraject
//
// Calculate a full ball trajectory from the current ball position (at a paddle
// bounce or start position) towards the far end (next paddle bounce or start
// position). The stored trajectory will then be played in subsequent gameplay
// ticks.
//
static void pongBallTraject(void)
{
  float ballX, ballY;
  float oldBallX, oldBallY;
  float dx, dy;
  float ballDx, ballDy;
  u08 tix = 0;
  u08 paddleColl = COLL_NONE;
  u08 bounceY;
  u08 keepoutTop, keepoutBot, ballEndY;
  u08 avoidPaddle;

  // Start trajectory at the last ball position and get ball motion vectors
  ballX = trajX[0] = trajX[ticksPlay];
  ballY = trajY[0] = trajY[ticksPlay];
  pongBallVector(&ballDx, &ballDy);

  // Configure target paddle and if we must avoid a ball paddle bounce
  if (ballDirX == 1)
  {
    paddle = PADDLE_RIGHT;
    avoidPaddle = hourChanged;
  }
  else
  {
    paddle = PADDLE_LEFT;
    avoidPaddle = minuteChanged;
  }

  // Add trajectory positions until we leave the play field or hit a paddle
  ballEndY = bounceY = 0;
  while ((s08)ballX >= 0 && (u08)ballX + BALL_WIDLEN * 2 < GLCD_XPIXELS)
  {
    //DEBUG(putstring("SIM  ball=["); uart_put_dec(trajX[tix]));
    //DEBUG(putstring(","); uart_put_dec(trajY[tix]); putstring("], tix="));
    //DEBUG(uart_put_dec(tix); putstring_nl(""));

    // To determine the callout area get first ball position behind paddle
    if (paddleColl == COLL_CONF && oldBallX < PADDLE_RIGHT_X + PADDLE_W &&
        oldBallX + BALL_WIDLEN * 2 > PADDLE_LEFT_X)
      ballEndY = ballY;

    // Base position for next ball trajectory entry
    tix++;
    oldBallX = ballX;
    oldBallY = ballY;
    ballX = ballX + ballDx;
    ballY = ballY + ballDy;

    // Check collision with right or left paddle
    if ((s08)ballX + BALL_WIDLEN * 2 >= PADDLE_RIGHT_X &&
        paddleColl == COLL_NONE)
    {
      // Prepare to determine exact collision position with right paddle
      paddleColl = COLL_REQ;
      dx = PADDLE_RIGHT_X - (oldBallX + BALL_WIDLEN * 2);
    }
    else if ((s08)ballX <= PADDLE_LEFT_X + PADDLE_W && paddleColl == COLL_NONE)
    {
      // Prepare to determine exact collision position with left paddle
      paddleColl = COLL_REQ;
      dx = PADDLE_LEFT_X + PADDLE_W - oldBallX;
    }
    if (paddleColl == COLL_REQ)
    {
      // Determine the vertical bounce position and ball bounce tick
      paddleColl = COLL_CONF;
      dy = (dx / ballDx) * ballDy;
      bounceY = pongBarBounce(oldBallY + dy, 0);
      paddleTick = tix;
      if (avoidPaddle == MC_FALSE)
      {
        // Set final ball position in bounce trajectory and bounce the ball x
        // direction in preparation for the next trajectory calculation
        trajX[tix] = oldBallX + dx;
        trajY[tix] = pongBarBounce(oldBallY + dy, &ballDy);
        ballDirX = -ballDirX;
        ballAngle = ANGLE_NEW;
        break;
      }
    }

    // Next ball position in trajectory
    ballY = pongBarBounce(ballY, &ballDy);
    trajX[tix] = ballX;
    trajY[tix] = ballY;
  }
  ticksPlay = tix;
  tickNow = 0;

  // Determine the ball presence area when it intersects with the paddle and
  // immediately right after that (in case we need to miss the ball)
  keepoutTop = keepoutBot = 0;
  if (avoidPaddle == MC_TRUE)
  {
    // We left the play field so set final trajectory ball to start position
    trajX[tix] = GLCD_XPIXELS / 2 - BALL_WIDLEN;
    trajY[tix] = GLCD_YPIXELS / 2 - BALL_WIDLEN;
    if (bounceY > ballEndY)
    {
      keepoutTop = ballEndY;
      keepoutBot = bounceY + BALL_WIDLEN * 2;
    }
    else
    {
      keepoutTop = bounceY;
      keepoutBot = ballEndY + BALL_WIDLEN * 2;
    }
  }
  DEBUG(putstring("TRAJ ball=["); uart_put_dec(trajX[tix]));
  DEBUG(putstring(","); uart_put_dec(trajY[tix]); putstring("], tix="));
  DEBUG(uart_put_dec(tix); putstring_nl(""));

  // Now we can calculate where the paddle should go
  DEBUG(putstring("TRAJ bounce="); uart_put_dec(bounceY));
  if (avoidPaddle == MC_FALSE)
  {
    // We want to hit the ball, so make it centered
    paddleY = bounceY + BALL_WIDLEN - PADDLE_H / 2;
  }
  else
  {
    // We lost the round so make sure the paddle -doesn't- hit the ball
    DEBUG(putstring(", miss="));
    if (keepoutTop < BAR_H + PADDLE_H + 2)
    {
      // The ball is near the top so put the paddle right below it
      paddleY = keepoutBot + 1;
      DEBUG(putstring("top"));
    }
    else if (keepoutBot > GLCD_YPIXELS - BAR_H - PADDLE_H - 2)
    {
      // The ball is near the bottom so put the paddle right above it
      paddleY = keepoutTop - PADDLE_H - 1;
      DEBUG(putstring("bot"));
    }
    else
    {
      // We're in the middle so randomly put the paddle above or under the ball
      if (pongRandGet(2))
        paddleY = keepoutTop - PADDLE_H - 1;
      else
        paddleY = keepoutBot + 1;
      DEBUG(putstring("mid"));
    }
  }
  // Make sure the target paddle stays in the vertical play field
  if (paddleY < BAR_H + 1)
    paddleY = BAR_H + 1;
  if (paddleY > GLCD_YPIXELS - PADDLE_H - BAR_H - 1)
    paddleY = GLCD_YPIXELS - PADDLE_H - BAR_H - 1;

  if (avoidPaddle == MC_TRUE)
  {
    DEBUG(putstring(", endpos="); uart_put_dec(ballEndY));
    DEBUG(putstring(", keepout="); uart_put_dec(keepoutTop));
    DEBUG(putstring(":"); uart_put_dec(keepoutBot));
  }
  DEBUG(putstring(", paddle="); uart_put_dec(paddleY));
  DEBUG(putstring(", bouncetix="); uart_put_dec(paddleTick));
  DEBUG(putstring(", playtix="); uart_put_dec(tix));
  DEBUG(putstring_nl(""));

  // Useful visual debugging in clock:
  // Draw dots for bounce and paddle position and the vertical keepout area
  /*u08 x;
  if (paddle == PADDLE_RIGHT)
    x = PADDLE_RIGHT_X + PADDLE_W + 1;
  else
    x = PADDLE_LEFT_X - 3;
  glcdColorSetBg();
  glcdFillRectangle(x, BAR_H, 2, GLCD_YPIXELS - BAR_H - BAR_H);
  glcdColorSetFg();
  glcdDot(x + 1, paddleY);
  glcdDot(x + 1, bounceY);
  if (avoidPaddle == MC_TRUE)
    glcdRectangle(x, keepoutTop, 1, keepoutBot - keepoutTop);*/
}

//
// Function: pongBallVector
//
// Get a random angle in range 0..pi/2 radials, excluding too steep angles, or
// keep current angle. Use angle to create x and y ball motion vectors but
// adjust them to the ball up/down and left/right direction indicators.
//
static void pongBallVector(float *ballDx, float *ballDy)
{
  float angleRad;

  if (ballAngle == ANGLE_NEW)
    ballAngle = pongRandGet(0) % (90 - BALL_ANGLE_MIN) + BALL_ANGLE_MIN;
  angleRad = (float)ballAngle * M_PI / 180;

  *ballDx = BALL_SPEED_MAX * sin(angleRad);
  if (*ballDx * ballDirX < 0)
    *ballDx = -*ballDx;
  *ballDy = BALL_SPEED_MAX * cos(angleRad);
  if (*ballDy * ballDirY < 0)
    *ballDy = -*ballDy;
  //DEBUG(putstring("VECT angle=");uart_put_dec(ballAngle));
  //DEBUG(putstring_nl(""));
}

//
// Function: pongBarBounce
//
// Bounce a ball against a top/bottom bar and flip its y direction on request.
// Return the (corrected) ball y position.
//
static float pongBarBounce(float y, float *ballDy)
{
  if (ballDy != 0)
  {
    // When bouncing at bottom or top bar flip y direction
    if (((s08)y + BALL_WIDLEN * 2 >= GLCD_YPIXELS - BAR_H && ballDirY == 1) ||
        ((s08)y <= BAR_H && ballDirY == -1))
    {
      *ballDy = -*ballDy;
      ballDirY = -ballDirY;
    }
  }

  // When ball is out of the vertical playfield correct y position
  if ((s08)y + BALL_WIDLEN * 2 > GLCD_YPIXELS - BAR_H)
    return GLCD_YPIXELS - BAR_H - BALL_WIDLEN * 2;
  else if ((s08)y < BAR_H)
    return BAR_H;
  else
    return y;
}

//
// Function: pongDrawBall
//
// Optionally remove the ball from its previous location and draw the ball at
// its new location
//
static void pongDrawBall(u08 remove)
{
  if (remove == MC_TRUE)
  {
    glcdColorSetBg();
    glcdFillRectangle(trajX[tickNow - 1], trajY[tickNow - 1], BALL_WIDLEN * 2,
       BALL_WIDLEN * 2);
    glcdColorSetFg();
  }
  glcdFillRectangle(trajX[tickNow], trajY[tickNow], BALL_WIDLEN * 2,
    BALL_WIDLEN * 2);
}

//
// Function: pongDrawBigDigit
//
// Draw a big digit by force or when intersected by the old ball location. Note
// that the bottom segment of a digit is two pixels taller, similar to the
// digits in the original Atari Pong.
//
static void pongDrawBigDigit(u08 x, u08 value, u08 high)
{
  u08 i, j;
  u08 segment;
  u08 extra = 0;

  if (scoreRedraw || pongBallIntersect(trajX[tickNow - 1], trajY[tickNow - 1],
      x, SCORE_TIME_Y, SCORE_DIGIT_W, SCORE_DIGIT_H))
  {
    // Determine to draw high or low digit of number and get digit fontbyte
    if (high == MC_TRUE)
      value = value / 10;
    else
      value = value % 10;
    segment = pgm_read_byte(bigFont + value);

    // Draw two vertical segments on the left and the right
    for (i = 0; i < 2; i++)
    {
      for (j = 0; j < 2; j++)
      {
        if (segment & 0x1)
          glcdColorSetFg();
        else
          glcdColorSetBg();
        glcdFillRectangle(x + i * 6, SCORE_TIME_Y + j * 7, 2, 9 + 2 * j);
        segment = segment >> 1;
      }
    }

    // Draw three horizontal segments
    for (i = 0; i < 3; i++)
    {
      if (i == 2)
        extra = 2;
      if (segment & 0x1)
      {
        glcdColorSetFg();
        glcdFillRectangle(x, SCORE_TIME_Y + i * 7 + extra, 8, 2);
      }
      else
      {
        glcdColorSetBg();
        glcdFillRectangle(x + 2, SCORE_TIME_Y + i * 7 + extra, 4, 2);
      }
      segment = segment >> 1;
    }
  }
  glcdColorSetFg();
}

//
// Function: pongDrawMidLine
//
// Draw a dashed vertical line in the middle of the play area
//
static void pongDrawMidLine(void)
{
  u08 i;

  for (i = 0; i < GLCD_YPIXELS / 8 - 1; i++)
  {
    glcdSetAddress((GLCD_XPIXELS - MIDLINE_W) / 2, i);
    if (mcFgColor)
      glcdDataWrite(0x0f);
    else
      glcdDataWrite(0xf0);
  }
  glcdSetAddress((GLCD_XPIXELS - MIDLINE_W) / 2, i);
  if (mcFgColor)
    glcdDataWrite(0xcf);
  else
    glcdDataWrite(0x30);
}

//
// Function: pongDrawPaddle
//
// Draw a paddle
//
static void pongDrawPaddle(s08 x, s08 oldY, s08 newY)
{
  // There are several options on redrawing a paddle (if needed anyway)
  if (oldY != newY || mcClockInit == MC_TRUE)
  {
    // Clear old paddle and draw new open or filled new paddle
    glcdColorSetBg();
    glcdFillRectangle(x, oldY, PADDLE_W, PADDLE_H);
    glcdColorSetFg();
    if (almDisplayState == MC_TRUE)
      glcdRectangle(x, newY, PADDLE_W, PADDLE_H);
    else
      glcdFillRectangle(x, newY, PADDLE_W, PADDLE_H);
  }
  else if (almDisplayState != mcU8Util1)
  {
    // Inverse centre of static paddle while alarming or end of alarm
    glcdFillRectangle2(x + 1, newY + 1, 1, PADDLE_H - 2, ALIGN_AUTO,
      FILL_INVERSE);
  }
}

//
// Function: pongDrawScore
//
// Draw the pong score being the time, date, year or alarm time
//
static void pongDrawScore(void)
{
  // Do admin on the display timeout counter and switch to next score mode when
  // timeout has occurred
  if (mcU8Util4 > 0)
  {
    mcU8Util4--;
    if (mcU8Util4 == 0)
    {
      switch (mcU8Util2)
      {
      case SCORE_MODE_ALARM:
        // Alarm time -> Time (default)
        DEBUGP("SCOR time");
        mcU8Util2 = SCORE_MODE_TIME;
        break;
      case SCORE_MODE_DATE:
        // Date -> Year
        DEBUGP("SCOR year");
        mcU8Util2 = SCORE_MODE_YEAR;
        mcU8Util4 = COUNTDOWN_SCORE;
        break;
      case SCORE_MODE_YEAR:
        if (mcAlarmSwitch == ALARM_SWITCH_ON)
        {
          // Year -> Alarm time
          DEBUGP("SCOR alarm");
          mcU8Util2 = SCORE_MODE_ALARM;
          mcU8Util4 = COUNTDOWN_SCORE;
        }
        else
        {
          // Year -> Time (default)
          DEBUGP("SCOR time");
          mcU8Util2 = SCORE_MODE_TIME;
        }
        break;
      }
    }
  }

  // If we have a new score mode sync it and force score redraw
  if (mcU8Util2 != mcU8Util3)
  {
    mcU8Util3 = mcU8Util2;
    scoreRedraw = MC_TRUE;
  }

  // Depending on the score mode set the left and right values
  if (scoreRedraw == MC_TRUE)
  {
    switch (mcU8Util2)
    {
    case SCORE_MODE_TIME:
      // Time hour + minute
      scoreLeft = mcClockNewTH;
      scoreRight = mcClockNewTM;
      break;
    case SCORE_MODE_DATE:
      // Month + Day
      scoreLeft = mcClockNewDM;
      scoreRight = mcClockNewDD;
      break;
    case SCORE_MODE_YEAR:
      // 20 + Year
      scoreLeft = 20;
      scoreRight = mcClockNewDY;
      break;
    case SCORE_MODE_ALARM:
      // Alarm hour + minute
      scoreLeft = mcAlarmH;
      scoreRight = mcAlarmM;
      break;
    }
  }

  // If needed draw left score two digits and right score two digits
  pongDrawBigDigit(SCORE_H10_X, scoreLeft, MC_TRUE);
  pongDrawBigDigit(SCORE_H1_X, scoreLeft, MC_FALSE);
  pongDrawBigDigit(SCORE_M10_X, scoreRight, MC_TRUE);
  pongDrawBigDigit(SCORE_M1_X, scoreRight, MC_FALSE);

  // Clear a redraw request
  scoreRedraw = MC_FALSE;
}

//
// Function: pongGameStep
//
// Determine the next position of the ball and target paddle
//
static void pongGameStep(void)
{
  // Calculate new ball trajectory if the current one is completed
  if (tickNow == ticksPlay)
  {
    if (ticksPlay != paddleTick)
    {
      // Ball moved out of the play field and is reset to its start position
      minuteChanged = hourChanged = MC_FALSE;
      scoreRedraw = MC_TRUE;
      countdown = COUNTDOWN_GAME;
    }
    pongBallTraject();
  }

  // If we're waiting for a new serve don't move the ball
  if (countdown > 0)
    return;

  // Move to next position in ball trajectory
  tickNow++;

  // Save old paddle position to determine if paddle must be redrawn, and move
  // paddle just-in-time
  if (paddle == PADDLE_RIGHT)
  {
    oldRightPaddleY = rightPaddleY;
    if (tickNow < paddleTick)
      rightPaddleY = pongPaddleMove(rightPaddleY);
  }
  else
  {
    oldLeftPaddleY = leftPaddleY;
    if (tickNow < paddleTick)
      leftPaddleY = pongPaddleMove(leftPaddleY);
  }
}

//
// Function: pongPaddleMove
//
// Determine the next paddle y position. The paddle will reach its target y
// position two playticks prior to bouncing or missing the ball.
//
static s08 pongPaddleMove(s08 y)
{
  s08 distance = y - paddleY;
  s08 delta = -distance;

  if (distance < 0)
    distance = -distance;
  if (distance > (paddleTick - tickNow - 2) * PADDLE_SPEED_MAX)
  {
    if (distance > PADDLE_SPEED_MAX)
    {
      if (paddleY > y)
        delta = PADDLE_SPEED_MAX;
      else
        delta = -PADDLE_SPEED_MAX;
    }
    /*DEBUG(putstring("PADL dist="); uart_put_sdec(distance));
    DEBUG(putstring(", travel="); uart_put_sdec(delta));
    DEBUG(putstring(", pos="); uart_put_dec(y + delta));
    DEBUG(putstring(", tix="); uart_put_dec(tickNow); putstring_nl(""));*/
    return y + delta;
  }
  return y;
}

//
// Function: pongRandGet
//
// Generate a random number of most likely abysmal quality
//
static u16 pongRandGet(u08 type)
{
  pongRandBase = (u16)(pongRandSeed * (pongRandVal + mcClockNewTM) * 213);
  pongRandVal = mcCycleCounter * pongRandSeed + pongRandBase;
  if (type == 2)
    return (pongRandVal >> 1) & 0x1;
  else
    return pongRandVal;
}
