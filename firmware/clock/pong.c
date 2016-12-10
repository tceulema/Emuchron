//*****************************************************************************
// Filename  : 'pong.c'
// Title     : Main animation and drawing code for MONOCHRON pong clock
//*****************************************************************************

#include <stdlib.h>
#include <math.h>
#ifdef EMULIN
#include "../emulator/stub.h"
#endif
#ifndef EMULIN
#include "../util.h"
#endif
#include "../ks0108.h"
#include "../monomain.h"
#include "../glcd.h"
#include "../anim.h"
#include "pong.h"

// This is a tradeoff between sluggish and too fast to see
#define MAX_BALL_SPEED		5	// Note this is in vector arith.
#define BALL_RADIUS		2	// In pixels

// This reduces the random angle degree range to 0+range .. 180-range
// per pi rad preventing to steep angles
#define MIN_BALL_ANGLE		35

// Pixel location of HOUR10 HOUR1 MINUTE10 and MINUTE1 digits
#define DISPLAY_H10_X		30
#define DISPLAY_H1_X		45
#define DISPLAY_M10_X		70
#define DISPLAY_M1_X		85

// How big our screen is in pixels
#define SCREEN_W		128
#define SCREEN_H		64

// Buffer space from the top
#define DISPLAY_TIME_Y		4

// The size of a digit in pixels (for math and intersect purposes)
#define DISPLAY_DIGITW		8
#define DISPLAY_DIGITH		16

// Paddle x locations
#define RIGHTPADDLE_X		(SCREEN_W - PADDLE_W - 10)
#define LEFTPADDLE_X		10

// Paddle size (in pixels) and max speed for AI
#define PADDLE_H		12
#define PADDLE_W		3
#define MAX_PADDLE_SPEED	5

// How thick the top and bottom lines are in pixels
#define BOTBAR_H		2
#define TOPBAR_H		2

// Specs of the middle line
#define MIDLINE_W		1
#define MIDLINE_H		(SCREEN_H / 16)	// how many 'stipples'

// Whether we are displaying time (99% of the time)
// alarm (for a few sec when alarm is switched on)
// date/year/alarm (for a few sec when a supported button is pressed)
#define SCORE_MODE_TIME		0
#define SCORE_MODE_DATE		1
#define SCORE_MODE_YEAR		2
#define SCORE_MODE_ALARM	3

// Time in app cycles (representing ~3 secs) for non-time info to be displayed
#define SCORE_TIMEOUT		(u08)(1000L / ANIM_TICK_CYCLE_MS * 3 + 0.5)

// Time in app cycles (representing ~2 sec) to wait before starting a new game
#define PLAY_COUNTDOWN		(u08)(1000L / ANIM_TICK_CYCLE_MS * 2 + 0.5)

// Uncomment this if you want to keep the ball always in the vertical centre
// of the display. Give it a try and you'll see what I mean...
//#define BALL_VCENTERED

extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcAlarming, mcAlarmH, mcAlarmM;
extern volatile uint8_t mcAlarmSwitch;
extern volatile uint8_t mcUpdAlarmSwitch;
extern volatile uint8_t mcCycleCounter;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;
// mcU8Util1 = flashing the paddles during alarm/snooze
// mcU8Util2 = score mode
// mcU8Util3 = previous score mode
// mcU8Util4 = score display timeout timer
extern volatile uint8_t mcU8Util1, mcU8Util2, mcU8Util3, mcU8Util4;

// The big digit font character set
static const unsigned char __attribute__ ((progmem)) bigFont[] =
{
  0xFF, 0x81, 0x81, 0xFF, // 0
  0x00, 0x00, 0x00, 0xFF, // 1
  0x9F, 0x91, 0x91, 0xF1, // 2
  0x91, 0x91, 0x91, 0xFF, // 3
  0xF0, 0x10, 0x10, 0xFF, // 4
  0xF1, 0x91, 0x91, 0x9F, // 5
  0xFF, 0x91, 0x91, 0x9F, // 6
  0x80, 0x80, 0x80, 0xFF, // 7
  0xFF, 0x91, 0x91, 0xFF, // 8 
  0xF1, 0x91, 0x91, 0xFF, // 9
  0x00, 0x00, 0x00, 0x00, // <space>
};

// Specific stuff for the pong clock
static uint8_t left_score, right_score;
static float ball_x, ball_y;
static float oldball_x, oldball_y;
static float ball_dx, ball_dy;
static int8_t rightpaddle_y, leftpaddle_y;
static int8_t oldleftpaddle_y, oldrightpaddle_y;
static int8_t rightpaddle_dy, leftpaddle_dy;
static uint8_t redraw_score = 0;
static uint8_t minute_changed, hour_changed;
static uint8_t countdown = 0;

// Random value for determining the direction of the elements
static uint16_t pongRandBase = M_PI * M_PI * 1000;
static const float pongRandSeed = 3.9147258617;
static uint16_t pongRandVal = 0x5A3C;

// The keepout is used to know where to -not- put the paddle.
// The 'bouncepos' is where we expect the ball's y-coord to be when
// it intersects with the paddle area.
static uint8_t right_keepout_top, right_keepout_bot, right_bouncepos, right_endpos;
static uint8_t left_keepout_top, left_keepout_bot, left_bouncepos, left_endpos;
static int8_t right_dest, left_dest, ticksremaining;

// Local function prototypes
static void initdisplay(void);
static void initanim(void);
static void step(void);
static void draw(void);
static void setscore(void);
static void draw_paddle(int8_t x, int8_t oldY, int8_t newY);
static void draw_score(uint8_t redraw_digits);
static uint8_t intersectrect(uint8_t x1, uint8_t y1, uint8_t w1, uint8_t h1,
  uint8_t x2, uint8_t y2, uint8_t w2, uint8_t h2);
static uint8_t calculate_keepout(float theball_x, float theball_y,
  float theball_dx, float theball_dy, uint8_t *keepout1, uint8_t *keepout2);
static void drawbigdigit(uint8_t x, uint8_t y, uint8_t n);
static void drawmidline(void);
static float random_angle_rads(void);
static uint16_t crand(uint8_t type);
static void pongAlarmUpdate(void);

//
// Function: pongButton
//
// Process pressed button for Pong clock
//
void pongButton(u08 pressedButton)
{
  // Set score to date (then year, (optionally) alarm and back to time)
  DEBUGP("Score -> Date");
  mcU8Util2 = SCORE_MODE_DATE;
  mcU8Util4 = SCORE_TIMEOUT + 1;
  setscore();
}

//
// Function: pongCycle
//
// Update the Pong clock per clock cycle
//
void pongCycle(void)
{
  // In case we're not displaying time do admin on the display timeout 
  // counter and switch to next mode when timeout has occurred
  if (mcU8Util4 > 0)
  {
    mcU8Util4--;
    if (mcU8Util4 == 0)
    {
      switch (mcU8Util2)
      {
      case SCORE_MODE_ALARM:
        // Alarm time -> Time (default)
        DEBUGP("Score -> Time");
        mcU8Util2 = SCORE_MODE_TIME;
        break;
      case SCORE_MODE_DATE:
        // Date -> Year
        DEBUGP("Score -> Year");
        mcU8Util2 = SCORE_MODE_YEAR;
        mcU8Util4 = SCORE_TIMEOUT;
        break;
      case SCORE_MODE_YEAR:
        if (mcAlarmSwitch == ALARM_SWITCH_ON)
        {
          // Year -> Alarm time
          DEBUGP("Score -> Alarm");
          mcU8Util2 = SCORE_MODE_ALARM;
          mcU8Util4 = SCORE_TIMEOUT;
        }
        else
        {
          // Year -> Time (default)
          DEBUGP("Score -> Time");
          mcU8Util2 = SCORE_MODE_TIME;
        }
        break;
      }
      setscore();
    }
  }

  // Process a change in the alarm switch
  pongAlarmUpdate();

  // Calculate next pong step
  step();

  // Update the pong clock
  draw();
}

//
// Function: pongInit
//
// Initialize the LCD display for use as a Pong clock
//
void pongInit(u08 mode)
{
  mcU8Util1 = GLCD_FALSE;
  initanim();
  initdisplay();
  pongAlarmUpdate();
}

//
// Function: crand
//
// A random generator of (most likely) low quality that is yet simple and small
//
static uint16_t crand(uint8_t type)
{
  pongRandBase = (int)(pongRandSeed * (pongRandVal + mcClockNewTM) * 213);
  pongRandVal = mcCycleCounter * pongRandSeed + pongRandBase;

  if (type == 1)
    return pongRandVal & 0x3;
  else if (type == 2)
    return (pongRandVal >> 1) & 0x1;
  else
    return pongRandVal;

  return 0;
}

//
// Function: setscore
//
// Set the values for the left and right score digits
//
static void setscore(void)
{
  if (mcU8Util2 != mcU8Util3)
  {
    // There is a new score mode so force a redraw
    redraw_score = 1;
    // Sync last with current mode
    mcU8Util3 = mcU8Util2;
  }

  // Depending on the score mode set the left and right values 
  switch (mcU8Util2)
  {
  case SCORE_MODE_TIME:
    // Time Hour + Time Minute
    left_score = mcClockNewTH;
    right_score = mcClockNewTM;
    break;
  case SCORE_MODE_DATE:
    // Month + Day
    left_score = mcClockNewDM;
    right_score = mcClockNewDD;
    break;
  case SCORE_MODE_YEAR:
    // 20 + Year
    left_score = 20;
    right_score = mcClockNewDY;
    break;
  case SCORE_MODE_ALARM:
    // Alarm Hour + Alarm Minute
    left_score = mcAlarmH;
    right_score = mcAlarmM;
    break;
  }
}

//
// Function: initanim
//
// Initialize the animation data
//
static void initanim(void)
{
  float angle = random_angle_rads();

  // Init everything for a smooth pong startup
  mcU8Util2 = mcU8Util3 = SCORE_MODE_TIME;
  mcU8Util4 = 0;
  rightpaddle_dy = leftpaddle_dy = 0;
  redraw_score = 0;
  minute_changed = hour_changed = 0;
  right_keepout_top = right_keepout_bot = right_bouncepos = right_endpos = 0;
  left_keepout_top = left_keepout_bot = left_bouncepos = left_endpos = 0;
  right_dest = left_dest = 0;
  ticksremaining = 0;
  countdown = PLAY_COUNTDOWN;

  // Start position of paddles
  leftpaddle_y = oldleftpaddle_y = 25;
  rightpaddle_y = oldrightpaddle_y = 25;

  // Start position of ball and its x/y speed
  ball_x = oldball_x = (SCREEN_W / 2) - 2;
  ball_y = oldball_y = (SCREEN_H / 2) - 2;
  ball_dx = MAX_BALL_SPEED * sin(angle);
  ball_dy = MAX_BALL_SPEED * cos(angle);
}

//
// Function: initdisplay
//
// Draw the initial pong clock
//
static void initdisplay(void)
{
  u08 newAlmDisplayState = GLCD_FALSE;
  
  if ((mcCycleCounter & 0x0F) >= 8)
    newAlmDisplayState = GLCD_TRUE;

  // Draw top and bottom line
  glcdFillRectangle(0, 0, GLCD_XPIXELS, 2, mcFgColor);
  glcdFillRectangle(0, GLCD_YPIXELS - 2, GLCD_XPIXELS, 2, mcFgColor);

  // Draw left and right paddle
  if (mcAlarming == GLCD_TRUE && newAlmDisplayState == GLCD_TRUE)
    glcdRectangle(LEFTPADDLE_X, leftpaddle_y, PADDLE_W, PADDLE_H, mcFgColor);
  else
    glcdFillRectangle(LEFTPADDLE_X, leftpaddle_y, PADDLE_W, PADDLE_H,
      mcFgColor);
  if (mcAlarming == GLCD_TRUE && newAlmDisplayState == GLCD_TRUE)
    glcdRectangle(RIGHTPADDLE_X, rightpaddle_y, PADDLE_W, PADDLE_H, mcFgColor);
  else
    glcdFillRectangle(RIGHTPADDLE_X, rightpaddle_y, PADDLE_W, PADDLE_H,
      mcFgColor);

  // Set initial alarm area state
  if (mcAlarming == GLCD_TRUE)
    mcU8Util1 = newAlmDisplayState;
  else
    mcU8Util1 = GLCD_FALSE;

  // Get 'score' values to draw (initially hour+minute)
  setscore();

  // Draw hours+minutes score
  drawbigdigit(DISPLAY_H10_X, DISPLAY_TIME_Y, left_score / 10);
  drawbigdigit(DISPLAY_H1_X, DISPLAY_TIME_Y, left_score % 10);
  drawbigdigit(DISPLAY_M10_X, DISPLAY_TIME_Y, right_score / 10);
  drawbigdigit(DISPLAY_M1_X, DISPLAY_TIME_Y, right_score % 10);

  // Draw dotted vertical line in middle
  drawmidline();
}

//
// Function: step
//
// Determine the next clock layout. In includes determining the next position
// of the ball and with that determine the next position of the paddles,
// signal ball bounces on the top/bottom of the play area and on the paddles,
// signal a change in hour/min (so a paddle should miss the ball) and signal
// the ball reaching the most left/right side of the field (so we restart the
// game).
//
static void step(void)
{
  float angle = 0;
  float dx, dy;
  int8_t distance;

  // Signal a change in minutes or hours
  if (mcClockTimeEvent == GLCD_TRUE && minute_changed == 0 &&
      hour_changed == 0)
  {
    // A change in hours has priority over change in minutes
    if (mcClockOldTH != mcClockNewTH)
    {
      DEBUGP("time -> hours");
      hour_changed = 1;
    }
    else if (mcClockOldTM != mcClockNewTM)
    {
      DEBUGP("time -> minutes");
      minute_changed = 1;
    }
  }

  //DEBUG(uart_putw_dec(mcClockTimeEvent));
  //DEBUG(putstring(", "));
  //DEBUG(uart_putw_dec(minute_changed));
  //DEBUG(putstring(", "));
  //DEBUG(uart_putw_dec(hour_changed));
  //DEBUG(putstring_nl(""));

  // If we're waiting for a new game to start we don't want to calculate
  // new positions
  if (countdown > 0)
    return;

  // Save old ball location so we can do some vector stuff 
  oldball_x = ball_x;
  oldball_y = ball_y;

  // Move ball according to the vector
  ball_x += ball_dx;
  ball_y += ball_dy;
    
  /************************************* TOP & BOTTOM WALLS */
  // Bouncing off bottom wall, reverse direction
  if (ball_y  > (SCREEN_H - BALL_RADIUS * 2 - BOTBAR_H))
  {
    //DEBUG(putstring_nl("bottom wall bounce"));
    ball_y = SCREEN_H - BALL_RADIUS * 2 - BOTBAR_H;
    ball_dy *= -1;
  }
  
  // Bouncing off top wall, reverse direction
  if (ball_y < TOPBAR_H)
  {
    //DEBUG(putstring_nl("top wall bounce"));
    ball_y = TOPBAR_H;
    ball_dy *= -1;
  }
  
  // For debugging, print the ball location
  /*DEBUG(putstring("ball @ ("));
  DEBUG(uart_putw_dec(ball_x));
  DEBUG(putstring(", "));
  DEBUG(uart_putw_dec(ball_y));
  DEBUG(putstring_nl(")"));*/

  /************************************* LEFT & RIGHT WALLS */
  // The ball hits either wall, the ball resets location
  if ((ball_x  > (SCREEN_W - BALL_RADIUS * 2)) || ((int8_t)ball_x <= 0))
  {
    if (DEBUGGING)
    {
      if ((int8_t)ball_x <= 0)
      {
        putstring("Left wall collide");
        if (minute_changed == 0)
          putstring_nl("...on accident");
        else
          putstring_nl("...on purpose");
      }
      else
      {
        putstring("Right wall collide");
        if (hour_changed == 0)
          putstring_nl("...on accident");
        else
          putstring_nl("...on purpose");
      }
    }

    // Place ball in the middle of the screen and re-use the same
    // x/y angles (so the ball moves to the paddle that just 'lost') 
    ball_x = (SCREEN_W / 2) - 2;
    ball_y = (SCREEN_H / 2) - 2;

    right_keepout_top = right_keepout_bot = 0;
    left_keepout_top = left_keepout_bot = 0;

    // Clear hour/minute change signal
    minute_changed = hour_changed = 0;

    // Set values for score display and force score redraw
    setscore();
    redraw_score = 1;

    // New serve: the game will be static for a few moment before it starts
    countdown = PLAY_COUNTDOWN;
    return;
  }

  // Save old paddle position
  oldleftpaddle_y = leftpaddle_y;
  oldrightpaddle_y = rightpaddle_y;

  /************************************* RIGHT PADDLE */
  // Check if we are bouncing off right paddle
  if (ball_dx > 0)
  {
    if ((((int8_t)ball_x + BALL_RADIUS * 2) >= RIGHTPADDLE_X) && 
        (((int8_t)oldball_x + BALL_RADIUS * 2) <= RIGHTPADDLE_X))
    {
      // Check if we collided
      DEBUG(putstring_nl("coll?"));

      // Determine the exact position at which it would collide
      dx = RIGHTPADDLE_X - (oldball_x + BALL_RADIUS * 2);
      // Now figure out what fraction that is of the motion and multiply
      // that by the dy
      dy = (dx / ball_dx) * ball_dy;
    
      if (intersectrect((oldball_x + dx), (oldball_y + dy), BALL_RADIUS * 2,
          BALL_RADIUS * 2, RIGHTPADDLE_X, rightpaddle_y, PADDLE_W, PADDLE_H))
      {
        if (DEBUGGING)
        {
          putstring_nl("nosect");
          if (hour_changed)
          {
            // uh oh
            putstring_nl("FAILED to miss");
          }
          putstring("RCOLLISION ball @ ("); uart_putw_dec(oldball_x + dx);
          putstring(", "); uart_putw_dec(oldball_y + dy);
          putstring(") & paddle @ [");
          uart_putw_dec(RIGHTPADDLE_X); putstring(", ");
          uart_putw_dec(rightpaddle_y); putstring("]");
        }
   
        // Set the ball right up against the paddle and bounce it
        ball_x = oldball_x + dx;
        ball_y = oldball_y + dy;

        // Bounce ball but set new angles for x and y
        // Put x in opposite direction and keep y in same direction
        angle = random_angle_rads();
        ball_dx = MAX_BALL_SPEED * sin(angle);
        if (ball_dx > 0)
          ball_dx *= -1;
        angle = MAX_BALL_SPEED * cos(angle);
        if (angle * ball_dy < 0)
          ball_dy = -angle;
        else
          ball_dy = angle;
      
        right_bouncepos = right_dest = right_keepout_top = right_keepout_bot = 0;
      }

      // Otherwise, it didn't bounce... will probably hit the right wall
      DEBUG(putstring(" tix = "));
      DEBUG(uart_putw_dec(ticksremaining));
      DEBUG(putstring_nl(""));
    }

    if (ball_dx > 0 && ((ball_x + BALL_RADIUS * 2) < RIGHTPADDLE_X))
    {
      // Ball is coming towards the right paddle
      if (right_keepout_top == 0)
      {
        ticksremaining = calculate_keepout(ball_x, ball_y, ball_dx, ball_dy,
          &right_bouncepos, &right_endpos);
        if (DEBUGGING)
        {
          putstring("Expect bounce @ "); uart_putw_dec(right_bouncepos); 
          putstring("-> thru to "); uart_putw_dec(right_endpos);
          putstring("\n\r");
        }
        if (right_bouncepos > right_endpos)
        {
          right_keepout_top = right_endpos;
          right_keepout_bot = right_bouncepos + BALL_RADIUS * 2;
        }
        else
        {
          right_keepout_top = right_bouncepos;
          right_keepout_bot = right_endpos + BALL_RADIUS * 2;
        }

        /*if(DEBUGGING)
        {
          putstring("Keepout from "); uart_putw_dec(right_keepout_top); 
          putstring(" to "); uart_putw_dec(right_keepout_bot); putstring_nl("");
        }*/
      
        // Now we can calculate where the paddle should go
        if (hour_changed == 0)
        {
          // We want to hit the ball, so make it centered.
          right_dest = right_bouncepos + BALL_RADIUS - PADDLE_H / 2;
          if (DEBUGGING)
          {
            putstring("hitR -> "); uart_putw_dec(right_dest); putstring_nl("");
          }
        }
        else
        {
          // We lost the round so make sure we -dont- hit the ball
          if (right_keepout_top <= (TOPBAR_H + PADDLE_H))
          {
            // The ball is near the top so make sure it ends up right below it
            //DEBUG(putstring_nl("at the top"));
            right_dest = right_keepout_bot + 1;
          }
          else if (right_keepout_bot >= (SCREEN_H - BOTBAR_H - PADDLE_H - 1))
          {
            // The ball is near the bottom so make sure it ends up right above it
            //DEBUG(putstring_nl("at the bottom"));
            right_dest = right_keepout_top - PADDLE_H - 1;
          }
          else
          {
            //DEBUG(putstring_nl("in the middle"));
            if (crand(2))
              right_dest = right_keepout_top - PADDLE_H - 1;
            else
              right_dest = right_keepout_bot + 1;
          }
        }
      }
      else
      {
        ticksremaining--;
      }

      // Draw the keepout area (for debugging)
      //glcdRectangle(RIGHTPADDLE_X, right_keepout_top, PADDLE_W,
      //  right_keepout_bot - right_keepout_top);

      distance = rightpaddle_y - right_dest;
    
      /*if(DEBUGGING)
      {
        putstring("dest dist: "); uart_putw_dec(abs(distance)); 
        putstring("\n\rtix: "); uart_putw_dec(ticksremaining);
        putstring("\n\rmax travel: "); uart_putw_dec(ticksremaining * MAX_PADDLE_SPEED);
        putstring_nl("");
      }*/

      // If we have just enough time, move the paddle!
      if (ABS(distance) > (ticksremaining - 1) * MAX_PADDLE_SPEED)
      {
        distance = ABS(distance);
        if (right_dest > rightpaddle_y)
        {
          if (distance > MAX_PADDLE_SPEED)
            rightpaddle_y += MAX_PADDLE_SPEED;
          else
            rightpaddle_y += distance;
        }
        if (right_dest < rightpaddle_y)
        {
          if (distance > MAX_PADDLE_SPEED)
            rightpaddle_y -= MAX_PADDLE_SPEED;
          else
            rightpaddle_y -= distance;
        }
      } 
    }
  }
  else
  {
    /************************************* LEFT PADDLE */
    // Check if we are bouncing off left paddle
    if ((((int8_t)ball_x) <= (LEFTPADDLE_X + PADDLE_W)) && 
        (((int8_t)oldball_x) >= (LEFTPADDLE_X + PADDLE_W)))
    {
      // Check if we collided
      DEBUG(putstring_nl("coll?"));

      // Check if we collided
      // Determine the exact position at which it would collide
      dx = (LEFTPADDLE_X + PADDLE_W) - oldball_x;

      // Now figure out what fraction that is of the motion and multiply
      // that by the dy
      dy = (dx / ball_dx) * ball_dy;

      if (intersectrect((oldball_x + dx), (oldball_y + dy), BALL_RADIUS * 2,
          BALL_RADIUS * 2, LEFTPADDLE_X, leftpaddle_y, PADDLE_W, PADDLE_H))
      {
        if (DEBUGGING)
        {
          if (minute_changed)
          {
            // uh oh
            putstring_nl("FAILED to miss");
          }
          putstring("LCOLLISION ball @ ("); uart_putw_dec(oldball_x + dx);
          putstring(", "); uart_putw_dec(oldball_y + dy);
          putstring(") & paddle @ ["); 
          uart_putw_dec(LEFTPADDLE_X); putstring(", ");
          uart_putw_dec(leftpaddle_y); putstring("]");
        }

        // Bounce ball but set new angles for x and y
        // Put x in opposite direction and keep y in same direction
        angle = random_angle_rads();
        ball_dx = MAX_BALL_SPEED * sin(angle);
        if (ball_dx < 0)
          ball_dx *= -1;
        angle = MAX_BALL_SPEED * cos(angle);
        if (angle * ball_dy < 0)
          ball_dy = -angle;
        else
          ball_dy = angle;

        if ((uint8_t)ball_x != LEFTPADDLE_X + PADDLE_W)
        {
          // Set the ball right up against the paddle
          ball_x = oldball_x + dx;
          ball_y = oldball_y + dy;
        }
        left_bouncepos = left_dest = left_keepout_top = left_keepout_bot = 0;
      } 
      if (DEBUGGING)
      {
        putstring(" tix = "); uart_putw_dec(ticksremaining);
        putstring_nl("");
      }
      // Otherwise, it didn't bounce...will probably hit the left wall
    }

    if ((ball_dx < 0) && (ball_x > (LEFTPADDLE_X + BALL_RADIUS*2)))
    {
      // Ball is coming towards the left paddle
      if (left_keepout_top == 0)
      {
        ticksremaining = calculate_keepout(ball_x, ball_y, ball_dx, ball_dy,
          &left_bouncepos, &left_endpos);
        if (DEBUGGING)
        {
          putstring("Expect bounce @ "); uart_putw_dec(left_bouncepos); 
          putstring("-> thru to "); uart_putw_dec(left_endpos);
          putstring("\n\r");
        }
      
        if (left_bouncepos > left_endpos)
        {
          left_keepout_top = left_endpos;
          left_keepout_bot = left_bouncepos + BALL_RADIUS * 2;
        }
        else
        {
          left_keepout_top = left_bouncepos;
          left_keepout_bot = left_endpos + BALL_RADIUS * 2;
        }
        //if(DEBUGGING)
        //{
        //  putstring("Keepout from "); uart_putw_dec(left_keepout_top); 
        //  putstring(" to "); uart_putw_dec(left_keepout_bot); putstring_nl("");
        //}
      
        // Now we can calculate where the paddle should go
        if (minute_changed == 0)
        {
          // We want to hit the ball, so make it centered.
          left_dest = left_bouncepos + BALL_RADIUS - PADDLE_H / 2;
          if (DEBUGGING)
          {
            putstring("hitL -> "); uart_putw_dec(left_dest); putstring_nl("");
          }
        }
        else
        {
          // We lost the round so make sure we -dont- hit the ball
          if (left_keepout_top <= (TOPBAR_H + PADDLE_H))
          {
            // The ball is near the top so make sure it ends up right below it
            //DEBUG(putstring_nl("at the top"));
            left_dest = left_keepout_bot + 1;
          }
          else if (left_keepout_bot >= (SCREEN_H - BOTBAR_H - PADDLE_H - 1))
          {
            // The ball is near the bottom so make sure it ends up right above it
            //DEBUG(putstring_nl("at the bottom"));
            left_dest = left_keepout_top - PADDLE_H - 1;
          }
          else
          {
            //DEBUG(putstring_nl("in the middle"));
            if (crand(2))
              left_dest = left_keepout_top - PADDLE_H - 1;
            else
              left_dest = left_keepout_bot + 1;
          }
          if (DEBUGGING)
          {
            putstring("missL -> "); uart_putw_dec(left_dest); putstring_nl("");
          }
        }
      } 
      else
      {
        ticksremaining--;
      }
      // Draw the keepout area (for debugging)
      //glcdRectangle(LEFTPADDLE_X, left_keepout_top, PADDLE_W,
      //  left_keepout_bot - left_keepout_top);

      distance = ABS((leftpaddle_y - left_dest));
  
      /*if (DEBUGGING)
      {
        putstring("\n\rdest dist: "); uart_putw_dec(ABS(distance)); 
        putstring("\n\rtix: "); uart_putw_dec(ticksremaining);
        putstring("\n\rmax travel: "); uart_putw_dec(ticksremaining * MAX_PADDLE_SPEED);
        putstring_nl("");
      }*/

      /*if (DEBUGGING)
      {
        putstring("\n\rleft paddle @ "); uart_putw_dec(leftpaddle_y); putstring_nl("");
      }*/

      // If we have just enough time, move the paddle!
      if (distance > ((ticksremaining - 1) * MAX_PADDLE_SPEED))
      {
        if (left_dest > leftpaddle_y)
        {
          if (distance > MAX_PADDLE_SPEED)
            leftpaddle_y += MAX_PADDLE_SPEED;
          else
            leftpaddle_y += distance;
        }
        if (left_dest < leftpaddle_y)
        {
          if (distance > MAX_PADDLE_SPEED)
            leftpaddle_y -= MAX_PADDLE_SPEED;
          else
            leftpaddle_y -= distance;
        }
      } 
    }
  }

  // Make sure the paddles dont hit the top or bottom
  if (leftpaddle_y < TOPBAR_H + 1)
    leftpaddle_y = TOPBAR_H + 1;
  if (rightpaddle_y < TOPBAR_H + 1)
    rightpaddle_y = TOPBAR_H + 1;
  
  if (leftpaddle_y > (SCREEN_H - PADDLE_H - BOTBAR_H - 1))
    leftpaddle_y = (SCREEN_H - PADDLE_H - BOTBAR_H - 1);
  if (rightpaddle_y > (SCREEN_H - PADDLE_H - BOTBAR_H - 1))
    rightpaddle_y = (SCREEN_H - PADDLE_H - BOTBAR_H - 1);
}

//
// Function: drawmidline
//
// Draw the dashed vertical line in the middle of the play area
//
static void drawmidline(void)
{
  uint8_t i;
  for (i = 0; i < (SCREEN_H / 8 - 1); i++)
  { 
    glcdSetAddress((SCREEN_W - MIDLINE_W) / 2, i);
    if (mcBgColor)
      glcdDataWrite(0xF0);
    else
      glcdDataWrite(0x0F);
  }
  glcdSetAddress((SCREEN_W - MIDLINE_W) / 2, i);
  if (mcBgColor)
    glcdDataWrite(0x30);
  else
    glcdDataWrite(0xCF);
}

//
// Function: draw
//
// Draw the new layout of the clock. It will remove ball from its old location
// and redraw it on its new location, it will redraw the paddles and flash
// its center upon alarming, it will update the score in case anything has
// changed. It will also redraw an unchanged score and middle line when they
// overlap with the old ball location.
//
static void draw(void)
{
  // Decide what to do with the ball
  if (countdown > 0)
  {
    // We're waiting for a new serve
    countdown--;

    // When needed remove the old ball at the edge of the display
    if (oldball_x != ball_x || oldball_y != ball_y)
    {
      // Clear old ball and sync with new location (at center of display)
      glcdFillRectangle(oldball_x, oldball_y, BALL_RADIUS * 2,
        BALL_RADIUS * 2, mcBgColor);
      oldball_x = ball_x;
      oldball_y = ball_y;
    }
  }
  else
  {
    // Regular game play so the ball moved from a to b

    // Erase old ball and draw new one
    glcdFillRectangle(oldball_x, oldball_y, BALL_RADIUS * 2, BALL_RADIUS * 2,
      mcBgColor);
    glcdFillRectangle(ball_x, ball_y, BALL_RADIUS * 2, BALL_RADIUS * 2,
      mcFgColor);

    // Draw middle lines around where the ball may have intersected it?
    if (intersectrect(oldball_x, oldball_y, BALL_RADIUS * 2, BALL_RADIUS * 2,
        SCREEN_W / 2 - MIDLINE_W, 0, MIDLINE_W, SCREEN_H))
    {
      // Redraw it since we had an intersection
      drawmidline();
    }
  }

#ifdef BALL_VCENTERED
  // Keep the ball always in the vertical centre of the display. This means
  // that the entire play field will shift vertically up or down to keep the
  // ball vertically centered. For this we'll use the lcd controller
  // 'startline' register that will do the vertical shift of lcd image data
  // in hardware.
  // The use of this feature in this clock is rather useless apart from the
  // fact that it nicely demonstrates what the startline register actually
  // does :-)
  u08 i;
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    glcdControlWrite(i, GLCD_START_LINE | 
      ((u08)ball_y + SCREEN_H / 2 + 2) % SCREEN_H);
#endif

  // Move the paddles
  draw_paddle(LEFTPADDLE_X, oldleftpaddle_y, leftpaddle_y);
  draw_paddle(RIGHTPADDLE_X, oldrightpaddle_y, rightpaddle_y);

  // Set flashing state for next cycle
  if (mcAlarming == GLCD_TRUE)
  {
    // The flashing state of the paddles in case of alarming
    if ((mcCycleCounter & 0x0F) >= 8)
      mcU8Util1 = GLCD_TRUE;
    else
      mcU8Util1 = GLCD_FALSE;
  }
  else
  {
    mcU8Util1 = GLCD_FALSE;
  }

  // Draw score and clear force redraw flag (if set anyway)
  draw_score(redraw_score);
  redraw_score = 0;
    
  // Redraw ball just in case it got (partially) covered by the score 
  glcdFillRectangle(ball_x, ball_y, BALL_RADIUS * 2, BALL_RADIUS * 2,
    mcFgColor);
}

//
// Function: draw_paddle
//
// Draw the left or right paddle
//
static void draw_paddle(int8_t x, int8_t oldY, int8_t newY)
{
  u08 newAlmDisplayState = GLCD_FALSE;
  u08 inverseAlarmArea = GLCD_FALSE;

  // The flashing state of the paddles in case of alarming
  if ((mcCycleCounter & 0x0F) >= 8)
    newAlmDisplayState = GLCD_TRUE;

  // Clear paddle when needed
  if (oldY != newY)
    glcdFillRectangle(x, oldY, PADDLE_W, PADDLE_H, mcBgColor);

  // There are quite a few options on redrawing a paddle
  if (oldY != newY)
  {
    // Draw filled or open paddle depending on alarm state
    if (mcAlarming == GLCD_TRUE && newAlmDisplayState == GLCD_TRUE)
      glcdRectangle(x, newY, PADDLE_W, PADDLE_H, mcFgColor);
    else
      glcdFillRectangle(x, newY, PADDLE_W, PADDLE_H, mcFgColor);
  }
  else if (mcAlarming == GLCD_TRUE && newAlmDisplayState != mcU8Util1)
  {
    // Paddle fill toggle while alarming
    inverseAlarmArea = GLCD_TRUE;
  }
  else if (mcAlarming == GLCD_FALSE && mcU8Util1 == GLCD_TRUE)
  {
    // Restore to full paddle after alarming has ended
    inverseAlarmArea = GLCD_TRUE;
  }

  // Invert centre of paddle if needed
  if (inverseAlarmArea == GLCD_TRUE)
    glcdFillRectangle2(x + 1, newY + 1, 1, PADDLE_H - 2, ALIGN_AUTO,
      FILL_INVERSE, mcFgColor);
}

//
// Function: intersectrect
//
// Determine whether two rectangles overlap one another.
// Returns value 1 when that is the case.
//
static uint8_t intersectrect(uint8_t x1, uint8_t y1, uint8_t w1, uint8_t h1,
  uint8_t x2, uint8_t y2, uint8_t w2, uint8_t h2)
{
  // Yer everyday intersection tester
  // Check x coord first
  if (x1 + w1 < x2)
    return 0;
  if (x2 + w2 < x1)
    return 0;

  // Check the y coord second
  if (y1 + h1 < y2)
    return 0;
  if (y2 + h2 < y1)
    return 0;

  return 1;
}

//
// Function: draw_score
//
// Draw the full score in case the score data has changed or when a score
// element intersects with the old ball location
//
static void draw_score(uint8_t redraw_digits)
{
  // Draw digits for 'hours'
  if (redraw_digits || intersectrect(oldball_x, oldball_y, BALL_RADIUS * 2,
      BALL_RADIUS * 2, DISPLAY_H10_X, DISPLAY_TIME_Y, DISPLAY_DIGITW,
      DISPLAY_DIGITH))
    drawbigdigit(DISPLAY_H10_X, DISPLAY_TIME_Y, left_score / 10);
  if (redraw_digits || intersectrect(oldball_x, oldball_y, BALL_RADIUS * 2,
      BALL_RADIUS * 2, DISPLAY_H1_X, DISPLAY_TIME_Y, DISPLAY_DIGITW,
      DISPLAY_DIGITH))
    drawbigdigit(DISPLAY_H1_X, DISPLAY_TIME_Y, left_score % 10);

  // Draw digits for 'minutes'
  if (redraw_digits || intersectrect(oldball_x, oldball_y, BALL_RADIUS * 2,
      BALL_RADIUS * 2, DISPLAY_M10_X, DISPLAY_TIME_Y, DISPLAY_DIGITW,
      DISPLAY_DIGITH))
    drawbigdigit(DISPLAY_M10_X, DISPLAY_TIME_Y, right_score / 10);
  if (redraw_digits || intersectrect(oldball_x, oldball_y, BALL_RADIUS * 2,
      BALL_RADIUS * 2, DISPLAY_M1_X, DISPLAY_TIME_Y, DISPLAY_DIGITW,
      DISPLAY_DIGITH))
    drawbigdigit(DISPLAY_M1_X, DISPLAY_TIME_Y, right_score % 10);
}

//
// Function: drawbigdigit
//
// Draw a single big digit
//
static void drawbigdigit(uint8_t x, uint8_t y, uint8_t n)
{
  uint8_t i, j;
  
  for (i = 0; i < 4; i++)
  {
    uint8_t d = pgm_read_byte(bigFont + (n * 4) + i);
    for (j = 0; j < 8; j++)
    {
      if (d & _BV(7 - j))
        glcdFillRectangle(x + i * 2, y + j * 2, 2, 2, mcFgColor);
      else
        glcdFillRectangle(x + i * 2, y + j * 2, 2, 2, mcBgColor);
    }
  }
}

//
// Function: random_angle_rads
//
// Get a random angle in range 0..pi radials
//
static float random_angle_rads(void)
{
  uint16_t angleRand = crand(0);
  uint8_t side = crand(2);
  float angle;

  if (DEBUGGING)
  {
    putstring("\n\rrand = "); uart_putw_dec(angleRand);
  }
  angleRand = angleRand % (180 - MIN_BALL_ANGLE * 2) + MIN_BALL_ANGLE;

  if (DEBUGGING)
  {
    putstring(" side = "); uart_putw_dec(side);
  }
  angle = (float)angleRand + side * 180;
  if (DEBUGGING)
  {
    putstring(" new ejection angle = "); uart_putw_dec(angle); putstring_nl("");
  }
  angle = angle * M_PI / 180;

  return angle;
}

//
// Function: calculate_keepout
//
// Simulate a ball bounce and determine where the ball will end up
// on the other side of the play field. We need this to determine
// where the receiving paddle should move to in order to bounce
// the ball (or to avoid it in case of a time change).
//
static uint8_t calculate_keepout(float theball_x, float theball_y,
  float theball_dx, float theball_dy, uint8_t *keepout1, uint8_t *keepout2)
{
  // "simulate" the ball bounce...its not optimized (yet)
  float sim_ball_y = theball_y;
  float sim_ball_x = theball_x;
  float sim_ball_dy = theball_dy;
  float sim_ball_dx = theball_dx;
  float dx, dy; 
  uint8_t tix = 0, collided = 0;

  while ((sim_ball_x < (RIGHTPADDLE_X + PADDLE_W)) &&
      ((sim_ball_x + BALL_RADIUS * 2) > LEFTPADDLE_X))
  {
    float old_sim_ball_x = sim_ball_x;
    float old_sim_ball_y = sim_ball_y;
    sim_ball_y += sim_ball_dy;
    sim_ball_x += sim_ball_dx;
  
    if (sim_ball_y > (int8_t)(SCREEN_H - BALL_RADIUS * 2 - BOTBAR_H))
    {
      sim_ball_y = SCREEN_H - BALL_RADIUS * 2 - BOTBAR_H;
      sim_ball_dy *= -1;
    }

    if (sim_ball_y < TOPBAR_H)
    {
      sim_ball_y = TOPBAR_H;
      sim_ball_dy *= -1;
    }
    
    if ((((int8_t)sim_ball_x + BALL_RADIUS * 2) >= RIGHTPADDLE_X) && 
         ((old_sim_ball_x + BALL_RADIUS * 2) < RIGHTPADDLE_X))
    {
      // Check if we collided with the right paddle
      // First determine the exact position at which it would collide
      dx = RIGHTPADDLE_X - (old_sim_ball_x + BALL_RADIUS * 2);
      // Now figure out what fraction that is of the motion and multiply that by the dy
      dy = (dx / sim_ball_dx) * sim_ball_dy;
      /*if (DEBUGGING)
      {
        putstring("RCOLL@ ("); uart_putw_dec(old_sim_ball_x + dx);
        putstring(", "); uart_putw_dec(old_sim_ball_y + dy);
      }*/
      *keepout1 = old_sim_ball_y + dy; 
      collided = 1;
    }
    else if (((int8_t)sim_ball_x <= (LEFTPADDLE_X + PADDLE_W)) && 
        (old_sim_ball_x > (LEFTPADDLE_X + PADDLE_W)))
    {
      // Check if we collided with the left paddle
      // First determine the exact position at which it would collide
      dx = (LEFTPADDLE_X + PADDLE_W) - old_sim_ball_x;
      // Now figure out what fraction that is of the motion and multiply that by the dy
      dy = (dx / sim_ball_dx) * sim_ball_dy;
      /*if (DEBUGGING)
      {
        putstring("LCOLL@ ("); uart_putw_dec(old_sim_ball_x + dx);
        putstring(", "); uart_putw_dec(old_sim_ball_y + dy);
      }*/
      *keepout1 = old_sim_ball_y + dy; 
      collided = 1;
    }

    if (!collided)
      tix++;
    /*if (DEBUGGING)
    {
      putstring("\tSIMball @ ["); uart_putw_dec(sim_ball_x);
      putstring(", "); uart_putw_dec(sim_ball_y); putstring_nl("]");
    }*/
  }

  *keepout2 = sim_ball_y;
  return tix;
}

//
// Function: pongAlarmUpdate
//
// Prepare updates in pong due to change in alarm switch
//
static void pongAlarmUpdate(void)
{
  if (mcUpdAlarmSwitch == GLCD_TRUE)
  {
    if (mcAlarmSwitch == ALARM_SWITCH_ON)
    {
      // Switch is switched to on so we may have to show the
      // alarm time. Only do so if we're not initializing pong.
      if (mcClockInit == GLCD_FALSE)
      {
        DEBUGP("Score -> Alarm");
        mcU8Util2 = SCORE_MODE_ALARM;
        mcU8Util4 = SCORE_TIMEOUT;
        setscore();
      } 
    }
    else
    {
      // Switch is switched to off so revert back to time mode
      if (mcU8Util2 != SCORE_MODE_TIME)
        DEBUGP("Score -> Time");
      mcU8Util2 = SCORE_MODE_TIME;
      mcU8Util4 = 0;
      setscore();
    }
  }
}

