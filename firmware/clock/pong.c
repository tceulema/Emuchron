//*****************************************************************************
// Filename  : 'pong.c'
// Title     : Main animation and drawing code for MONOCHRON pong clock
//*****************************************************************************

#include <stdlib.h>
#include <math.h>
#ifdef EMULIN
#include "../emulator/stub.h"
#else
#include "../util.h"
#endif
#include "../ks0108.h"
#include "../monomain.h"
#include "../glcd.h"
#include "../anim.h"
#include "pong.h"

// How big our screen is in pixels
#define SCREEN_W		128
#define SCREEN_H		64

// This is a tradeoff between sluggish and too fast to see
#define MAX_BALL_SPEED		5	// Note this is in vector arith.
#define BALL_RADIUS		2	// In pixels

// This reduces the random angle degree range to 0+range .. 180-range
// per pi rad, preventing too steep angles
#define MIN_BALL_ANGLE		35

// Pixel location of HOUR10 HOUR1 MINUTE10 and MINUTE1 digits
#define DISPLAY_TIME_Y		4
#define DISPLAY_H10_X		30
#define DISPLAY_H1_X		45
#define DISPLAY_M10_X		70
#define DISPLAY_M1_X		85

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

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
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

// The big digit font character set describes seven segments bits per digit.
// Per bit in the font byte:
// 0-midbottom-midcenter-midtop-rightbottom-righttop-leftbottom-lefttop
static const unsigned char __attribute__ ((progmem)) bigFont[] =
{
  0x5f, // 0
  0x0c, // 1
  0x76, // 2
  0x7c, // 3
  0x2d, // 4
  0x79, // 5
  0x7b, // 6
  0x1c, // 7
  0x7f, // 8
  0x7d  // 9
};

// Specific stuff for the pong clock
static uint8_t left_score, right_score;
static float ball_x, ball_y;
static float oldball_x, oldball_y;
static float ball_dx, ball_dy;
static int8_t rightpaddle_y, leftpaddle_y;
static int8_t oldleftpaddle_y, oldrightpaddle_y;
static int8_t rightpaddle_dy, leftpaddle_dy;
static uint8_t redraw_score = GLCD_FALSE;
static uint8_t minute_changed, hour_changed;
static uint8_t countdown = 0;

// Random value for determining the direction of the ball
static uint16_t pongRandBase = M_PI * M_PI * 1000;
static const float pongRandSeed = 3.9147258617;
static uint16_t pongRandVal = 0x5A3C;

// The keepout is used to know where to -not- put the paddle.
// The 'bouncepos' is where we expect the ball's y-coord to be when it
// intersects with the paddle area.
static uint8_t right_keepout_top, right_keepout_bot, right_bouncepos, right_endpos;
static uint8_t left_keepout_top, left_keepout_bot, left_bouncepos, left_endpos;
static int8_t right_dest, left_dest, ticksremaining;

// Local function prototypes
static void bounce_ball(int8_t side);
static uint8_t calc_keepout(uint8_t *keepout1, uint8_t *keepout2);
static uint16_t crand(uint8_t type);
static void draw(void);
static void draw_ball(uint8_t remove);
static void draw_bigdigit(uint8_t x, uint8_t n, uint8_t high);
static void draw_paddle(int8_t x, int8_t oldY, int8_t newY);
static void draw_score(uint8_t redraw_digits);
static void draw_midline(void);
static void init_anim(void);
static void init_display(void);
static uint8_t intersectrect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
  uint8_t w2, uint8_t h2);
static int8_t paddle_pos(int8_t dest, int8_t paddle_y);
static float random_angle_rads(void);
static void set_score(void);
static void step(void);
static void update_alarm(void);

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
  set_score();
}

//
// Function: pongCycle
//
// Update the lcd display of a pong clock
//
void pongCycle(void)
{
  // In case we're not displaying time do admin on the display timeout counter
  // and switch to next mode when timeout has occurred
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
      set_score();
    }
  }

  // Process a change in the alarm switch, calculate next pong step and update
  // the pong clock
  update_alarm();
  step();
  draw();
}

//
// Function: pongInit
//
// Initialize the lcd display of a pong clock
//
void pongInit(u08 mode)
{
  mcU8Util1 = GLCD_FALSE;
  init_anim();
  init_display();
  update_alarm();
}

//
// Function: bounce_ball
//
// Bounce a ball against a paddle. The side parameter determines whether it is
// the left (-1) or right (1) paddle.
//
static void bounce_ball(int8_t side)
{
  float angle = 0;

  // The calculated ball paddle hit y position may be out of the play field
  // when the actual y position was very near the top/bottom bar so we must
  // compensate for that
  if (ball_y > SCREEN_H - BALL_RADIUS * 2 - BOTBAR_H)
    ball_y = SCREEN_H - BALL_RADIUS * 2 - BOTBAR_H;
  else if (ball_y < TOPBAR_H)
    ball_y = TOPBAR_H;

  // Bounce ball but set new angles for x and y. Put x in opposite direction
  // and keep y in same direction.
  angle = random_angle_rads();
  ball_dx = MAX_BALL_SPEED * sin(angle);
  if (ball_dx * side > 0)
    ball_dx = -ball_dx;
  angle = MAX_BALL_SPEED * cos(angle);
  if (angle * ball_dy < 0)
    ball_dy = -angle;
  else
    ball_dy = angle;
}

//
// Function: calc_keepout
//
// Simulate a ball bounce and determine where the ball will end up on the other
// side of the play field. We need this to determine where the receiving paddle
// should move to in order to bounce the ball (or to avoid it in case of a time
// change).
//
static uint8_t calc_keepout(uint8_t *keepout1, uint8_t *keepout2)
{
  float sim_ball_x = ball_x;
  float sim_ball_y = ball_y;
  float sim_ball_dx = ball_dx;
  float sim_ball_dy = ball_dy;
  float dx, dy;
  float old_sim_ball_x;
  float old_sim_ball_y;
  uint8_t tix = 0;
  uint8_t collided = GLCD_FALSE;

  while (sim_ball_x < RIGHTPADDLE_X + PADDLE_W &&
      sim_ball_x + BALL_RADIUS * 2 > LEFTPADDLE_X)
  {
    old_sim_ball_x = sim_ball_x;
    old_sim_ball_y = sim_ball_y;
    sim_ball_y += sim_ball_dy;
    sim_ball_x += sim_ball_dx;

    // Check bounce at bottom or top bar
    if (sim_ball_y > SCREEN_H - BALL_RADIUS * 2 - BOTBAR_H)
    {
      sim_ball_y = SCREEN_H - BALL_RADIUS * 2 - BOTBAR_H;
      sim_ball_dy = -sim_ball_dy;
    }
    else if (sim_ball_y < TOPBAR_H)
    {
      sim_ball_y = TOPBAR_H;
      sim_ball_dy = -sim_ball_dy;
    }

    // Check colliding with right or left paddle
    if ((int8_t)sim_ball_x + BALL_RADIUS * 2 >= RIGHTPADDLE_X &&
        collided == GLCD_FALSE)
    {
      // Colliding with the right paddle.
      // Determine the exact position at which it would collide.
      collided = GLCD_TRUE;
      dx = RIGHTPADDLE_X - (old_sim_ball_x + BALL_RADIUS * 2);
      dy = (dx / sim_ball_dx) * sim_ball_dy;
      *keepout1 = old_sim_ball_y + dy;
      /*if (DEBUGGING)
      {
        putstring("RCOLL@ ("); uart_putw_dec(old_sim_ball_x + dx);
        putstring(", "); uart_putw_dec(old_sim_ball_y + dy);
      }*/
    }
    else if ((int8_t)sim_ball_x <= LEFTPADDLE_X + PADDLE_W &&
        collided == GLCD_FALSE)
    {
      // Colliding with the left paddle.
      // Determine the exact position at which it would collide.
      collided = GLCD_TRUE;
      dx = (LEFTPADDLE_X + PADDLE_W) - old_sim_ball_x;
      dy = (dx / sim_ball_dx) * sim_ball_dy;
      *keepout1 = old_sim_ball_y + dy;
      /*if (DEBUGGING)
      {
        putstring("LCOLL@ ("); uart_putw_dec(old_sim_ball_x + dx);
        putstring(", "); uart_putw_dec(old_sim_ball_y + dy);
      }*/
    }

    // When not collided yet we need another cycle
    if (collided == GLCD_FALSE)
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
    // We're waiting for a new serve. When needed erase old ball at the edge of
    // the display and redraw at its serve location.
    countdown--;
    if (oldball_x != ball_x)
      draw_ball(GLCD_TRUE);
  }
  else
  {
    // Regular game play so the ball moved from a to b. Draw ball and redraw
    // middle line when intersected by the old ball.
    draw_ball(GLCD_TRUE);
    if (intersectrect(oldball_x, oldball_y, SCREEN_W / 2 - MIDLINE_W, 0,
        MIDLINE_W, SCREEN_H))
      draw_midline();
  }

#ifdef BALL_VCENTERED
  // Keep the ball always in the vertical centre of the display. This means
  // that the entire play field will shift vertically up or down to keep the
  // ball vertically centered. For this we'll use the lcd controller
  // 'startline' register that will do the vertical shift of lcd image data
  // in hardware.
  // The BALL_VCENTERED functionality in this clock is rather useless apart
  // from the fact that it nicely demonstrates what the lcd controller
  // startline register actually does :-)
  u08 i;
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    glcdControlWrite(i, GLCD_START_LINE |
      ((u08)ball_y + SCREEN_H / 2 + 2) % SCREEN_H);
#endif

  // Move the paddles
  draw_paddle(LEFTPADDLE_X, oldleftpaddle_y, leftpaddle_y);
  draw_paddle(RIGHTPADDLE_X, oldrightpaddle_y, rightpaddle_y);

  // Set flashing state for next cycle
  if (mcAlarming == GLCD_TRUE && (mcCycleCounter & 0x08) == 8)
    mcU8Util1 = GLCD_TRUE;
  else
    mcU8Util1 = GLCD_FALSE;

  // Draw score and clear force redraw flag (if set anyway)
  draw_score(redraw_score);
  redraw_score = GLCD_FALSE;

  // Redraw ball just in case it got (partially) removed by the score draw
  draw_ball(GLCD_FALSE);
}

//
// Function: draw_ball
//
// Optionally remove the ball from its previous location and draw the ball at
// its new location
//
static void draw_ball(uint8_t remove)
{
  if (remove == GLCD_TRUE)
    glcdFillRectangle(oldball_x, oldball_y, BALL_RADIUS * 2, BALL_RADIUS * 2,
       mcBgColor);
  glcdFillRectangle(ball_x, ball_y, BALL_RADIUS * 2, BALL_RADIUS * 2,
    mcFgColor);
}

//
// Function: draw_bigdigit
//
// Draw a single big digit
//
static void draw_bigdigit(uint8_t x, uint8_t n, uint8_t high)
{
  uint8_t i, j;
  uint8_t segment;

  // Determine to draw high or low digit of number
  if (high == GLCD_TRUE)
    n = n / 10;
  else
    n = n % 10;

  // The fontbyte contains the 7 segments of the digit
  segment = pgm_read_byte(bigFont + n);

  // Draw two vertical segments on the left and the right
  for (i = 0; i < 2; i++)
  {
    for (j = 0; j < 2; j++)
    {
      if (segment & 0x1)
        glcdFillRectangle(x + i * 6, DISPLAY_TIME_Y + j * 7, 2, 9, mcFgColor);
      else
        glcdFillRectangle(x + i * 6, DISPLAY_TIME_Y + j * 7, 2, 9, mcBgColor);
      segment = segment >> 1;
    }
  }

  // Draw three horizontal segments
  for (i = 0; i < 3; i++)
  {
    if (segment & 0x1)
      glcdFillRectangle(x, DISPLAY_TIME_Y + i * 7, 8, 2, mcFgColor);
    else
      glcdFillRectangle(x + 2, DISPLAY_TIME_Y + i * 7, 4, 2, mcBgColor);
    segment = segment >> 1;
  }
}

//
// Function: draw_midline
//
// Draw the dashed vertical line in the middle of the play area
//
static void draw_midline(void)
{
  uint8_t i;
  for (i = 0; i < (SCREEN_H / 8 - 1); i++)
  {
    glcdSetAddress((SCREEN_W - MIDLINE_W) / 2, i);
    if (mcBgColor)
      glcdDataWrite(0xf0);
    else
      glcdDataWrite(0x0f);
  }
  glcdSetAddress((SCREEN_W - MIDLINE_W) / 2, i);
  if (mcBgColor)
    glcdDataWrite(0x30);
  else
    glcdDataWrite(0xcf);
}

//
// Function: draw_paddle
//
// Draw the left or right paddle
//
static void draw_paddle(int8_t x, int8_t oldY, int8_t newY)
{
  u08 newAlmDisplayState = GLCD_FALSE;

  // The flashing state of the paddles in case of alarming
  if (mcAlarming == GLCD_TRUE && (mcCycleCounter & 0x08) == 8)
    newAlmDisplayState = GLCD_TRUE;

  // There are quite a few options on redrawing a paddle
  if (oldY != newY)
  {
    // Clear old paddle
    glcdFillRectangle(x, oldY, PADDLE_W, PADDLE_H, mcBgColor);

    // Draw open or filled new paddle depending on alarm state
    if (newAlmDisplayState == GLCD_TRUE)
      glcdRectangle(x, newY, PADDLE_W, PADDLE_H, mcFgColor);
    else
      glcdFillRectangle(x, newY, PADDLE_W, PADDLE_H, mcFgColor);
  }
  else if (newAlmDisplayState != mcU8Util1)
  {
    // Inverse centre of paddle while alarming or end of alarm
    glcdFillRectangle2(x + 1, newY + 1, 1, PADDLE_H - 2, ALIGN_AUTO,
      FILL_INVERSE, mcFgColor);
  }
}

//
// Function: draw_score
//
// Draw the full score in case the score data has changed or when a score
// element intersects with the old ball location
//
static void draw_score(uint8_t redraw_digits)
{
  // Draw left two digits (usually hours)
  if (redraw_digits || intersectrect(oldball_x, oldball_y, DISPLAY_H10_X,
      DISPLAY_TIME_Y, DISPLAY_DIGITW, DISPLAY_DIGITH))
    draw_bigdigit(DISPLAY_H10_X, left_score, GLCD_TRUE);
  if (redraw_digits || intersectrect(oldball_x, oldball_y, DISPLAY_H1_X,
      DISPLAY_TIME_Y, DISPLAY_DIGITW, DISPLAY_DIGITH))
    draw_bigdigit(DISPLAY_H1_X, left_score, GLCD_FALSE);

  // Draw right two digits (usually minutes)
  if (redraw_digits || intersectrect(oldball_x, oldball_y, DISPLAY_M10_X,
      DISPLAY_TIME_Y, DISPLAY_DIGITW, DISPLAY_DIGITH))
    draw_bigdigit(DISPLAY_M10_X, right_score, GLCD_TRUE);
  if (redraw_digits || intersectrect(oldball_x, oldball_y, DISPLAY_M1_X,
      DISPLAY_TIME_Y, DISPLAY_DIGITW, DISPLAY_DIGITH))
    draw_bigdigit(DISPLAY_M1_X, right_score, GLCD_FALSE);
}

//
// Function: init_anim
//
// Initialize the animation data
//
static void init_anim(void)
{
  float angle = random_angle_rads();

  // Init everything for a smooth pong startup
  mcU8Util2 = mcU8Util3 = SCORE_MODE_TIME;
  mcU8Util4 = 0;
  rightpaddle_dy = leftpaddle_dy = 0;
  redraw_score = GLCD_FALSE;
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
// Function: init_display
//
// Draw the initial pong clock
//
static void init_display(void)
{
  // Set initial alarm area state
  if (mcAlarming == GLCD_TRUE && (mcCycleCounter & 0x08) == 8)
    mcU8Util1 = GLCD_TRUE;
  else
    mcU8Util1 = GLCD_FALSE;

  // Draw top and bottom line
  glcdFillRectangle(0, 0, GLCD_XPIXELS, 2, mcFgColor);
  glcdFillRectangle(0, GLCD_YPIXELS - 2, GLCD_XPIXELS, 2, mcFgColor);

  // Draw left and right paddle
  draw_paddle(LEFTPADDLE_X, leftpaddle_y - 1, leftpaddle_y);
  draw_paddle(RIGHTPADDLE_X, rightpaddle_y - 1, rightpaddle_y);

  // Set 'score' values to time (hour+minute) and draw it
  set_score();
  draw_score(GLCD_TRUE);

  // Draw dotted vertical line in middle
  draw_midline();
}

//
// Function: intersectrect
//
// Determine whether the ball rectangle overlaps with another rectangle.
// Returns value 1 when that is the case.
//
static uint8_t intersectrect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
  uint8_t w2, uint8_t h2)
{
  // Yer everyday intersection tester
  // Check x coord first
  if (x1 + BALL_RADIUS * 2 < x2)
    return 0;
  if (x2 + w2 < x1)
    return 0;

  // Check the y coord second
  if (y1 + BALL_RADIUS * 2 < y2)
    return 0;
  if (y2 + h2 < y1)
    return 0;

  return 1;
}

//
// Function: paddle_pos
//
// Determine the next paddle vertical position
//
static int8_t paddle_pos(int8_t dest, int8_t paddle_y)
{
  int8_t distance = paddle_y - dest;

  if (distance < 0)
    distance = -distance;
  if (distance > (ticksremaining - 1) * MAX_PADDLE_SPEED)
  {
    if (dest > paddle_y)
    {
      if (distance > MAX_PADDLE_SPEED)
        paddle_y += MAX_PADDLE_SPEED;
      else
        paddle_y += distance;
    }
    if (dest < paddle_y)
    {
      if (distance > MAX_PADDLE_SPEED)
        paddle_y -= MAX_PADDLE_SPEED;
      else
        paddle_y -= distance;
    }
  }
  return paddle_y;
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

  DEBUG(putstring("\n\rrand = "));
  DEBUG(uart_putw_dec(angleRand));
  angleRand = angleRand % (180 - MIN_BALL_ANGLE * 2) + MIN_BALL_ANGLE;

  DEBUG(putstring(" side = "));
  DEBUG(uart_putw_dec(side));

  angle = (float)angleRand + side * 180;
  DEBUG(putstring(" new ejection angle = "));
  DEBUG(uart_putw_dec(angle));
  DEBUG(putstring_nl(""));

  return angle * M_PI / 180;
}

//
// Function: set_score
//
// Set the values for the left and right score digits
//
static void set_score(void)
{
  if (mcU8Util2 != mcU8Util3)
  {
    // There is a new score mode so force a redraw
    redraw_score = GLCD_TRUE;
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
// Function: step
//
// Determine the next clock layout. In includes determining the next position
// of the ball and with that determine the next position of the paddles, signal
// ball bounces on the top/bottom of the play area and on the paddles, signal
// a change in hour/min (so a paddle should miss the ball) and signal the ball
// reaching the most left/right side of the field (so we restart the game).
//
static void step(void)
{
  float dx, dy;

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

  // Save old ball location so we can do some vector stuff
  oldball_x = ball_x;
  oldball_y = ball_y;

  // If we're waiting for a new game to start we don't want to calculate new
  // positions
  if (countdown > 0)
    return;

  // Move ball according to the vector
  ball_x += ball_dx;
  ball_y += ball_dy;

  /************************************* TOP & BOTTOM BARS */
  // Check bounce at bottom or top bar
  if (ball_y > SCREEN_H - BALL_RADIUS * 2 - BOTBAR_H)
  {
    //DEBUG(putstring_nl("bottom wall bounce"));
    ball_y = SCREEN_H - BALL_RADIUS * 2 - BOTBAR_H;
    ball_dy = -ball_dy;
  }
  else if (ball_y < TOPBAR_H)
  {
    //DEBUG(putstring_nl("top wall bounce"));
    ball_y = TOPBAR_H;
    ball_dy = -ball_dy;
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

    // Place ball in the middle of the screen and re-use the same x/y angles
    // (so the ball moves to the paddle that just 'lost')
    ball_x = (SCREEN_W / 2) - 2;
    ball_y = (SCREEN_H / 2) - 2;

    right_keepout_top = right_keepout_bot = 0;
    left_keepout_top = left_keepout_bot = 0;

    // Clear hour/minute change signal
    minute_changed = hour_changed = 0;

    // Set values for score display and force score redraw
    set_score();
    redraw_score = GLCD_TRUE;

    // New serve: the game will be static for a few moments before it starts
    countdown = PLAY_COUNTDOWN;
    return;
  }

  // Save old paddle position
  oldleftpaddle_y = leftpaddle_y;
  oldrightpaddle_y = rightpaddle_y;

  if (ball_dx > 0)
  {
    /************************************* RIGHT PADDLE */
    // Check if we are bouncing off right paddle
    if ((int8_t)ball_x + BALL_RADIUS * 2 >= RIGHTPADDLE_X &&
        (int8_t)oldball_x + BALL_RADIUS * 2 <= RIGHTPADDLE_X)
    {
      // Check if we collided
      DEBUG(putstring_nl("coll?"));

      // Determine the exact position at which it would collide
      dx = RIGHTPADDLE_X - (oldball_x + BALL_RADIUS * 2);
      dy = (dx / ball_dx) * ball_dy;

      if (intersectrect(oldball_x + dx, oldball_y + dy, RIGHTPADDLE_X,
          rightpaddle_y, PADDLE_W, PADDLE_H))
      {
        if (DEBUGGING)
        {
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

        // Prepare to set the ball right up against the paddle
        ball_x = oldball_x + dx;
        ball_y = oldball_y + dy;

        // Bounce ball and set new angles for x and y.
        // Put x in opposite direction and keep y in same direction.
        bounce_ball(1);
        right_keepout_top = 0;
      }

      // Otherwise, it didn't bounce... will probably hit the right wall
      DEBUG(putstring(" tix = "));
      DEBUG(uart_putw_dec(ticksremaining));
      DEBUG(putstring_nl(""));
    }

    if (ball_dx > 0 && ball_x + BALL_RADIUS * 2 < RIGHTPADDLE_X)
    {
      // Ball is coming towards the right paddle
      if (right_keepout_top == 0)
      {
        ticksremaining = calc_keepout(&right_bouncepos, &right_endpos);
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
        //if(DEBUGGING)
        //{
        //  putstring("Keepout from "); uart_putw_dec(right_keepout_top);
        //  putstring(" to "); uart_putw_dec(right_keepout_bot); putstring_nl("");
        //}

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
          if (right_keepout_top <= TOPBAR_H + PADDLE_H)
          {
            // The ball is near the top so make sure it ends up right below it
            //DEBUG(putstring_nl("at the top"));
            right_dest = right_keepout_bot + 1;
          }
          else if (right_keepout_bot >= SCREEN_H - BOTBAR_H - PADDLE_H - 1)
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

      /*if(DEBUGGING)
      {
        putstring("dest dist: "); uart_putw_dec(ABS(rightpaddle_y - right_dest));
        putstring("\n\rtix: "); uart_putw_dec(ticksremaining);
        putstring("\n\rmax travel: "); uart_putw_dec(ticksremaining * MAX_PADDLE_SPEED);
        putstring_nl("");
      }*/

      // If we have just enough time, move the paddle
      rightpaddle_y = paddle_pos(right_dest, rightpaddle_y);
    }
  }
  else
  {
    /************************************* LEFT PADDLE */
    // Check if we are bouncing off left paddle
    if ((int8_t)ball_x <= LEFTPADDLE_X + PADDLE_W &&
        (int8_t)oldball_x >= LEFTPADDLE_X + PADDLE_W)
    {
      // Check if we collided
      DEBUG(putstring_nl("coll?"));

      // Determine the exact position at which it would collide
      dx = (LEFTPADDLE_X + PADDLE_W) - oldball_x;
      dy = (dx / ball_dx) * ball_dy;

      if (intersectrect(oldball_x + dx, oldball_y + dy, LEFTPADDLE_X,
          leftpaddle_y, PADDLE_W, PADDLE_H))
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

        // Prepare to set the ball right up against the paddle
        ball_x = oldball_x + dx;
        ball_y = oldball_y + dy;

        // Bounce ball and set new angles for x and y.
        // Put x in opposite direction and keep y in same direction.
        bounce_ball(-1);
        left_keepout_top = 0;
      }

      // Otherwise, it didn't bounce...will probably hit the left wall
      DEBUG(putstring(" tix = "));
      DEBUG(uart_putw_dec(ticksremaining));
      DEBUG(putstring_nl(""));
    }

    if (ball_dx < 0 && ball_x > LEFTPADDLE_X + BALL_RADIUS * 2)
    {
      // Ball is coming towards the left paddle
      if (left_keepout_top == 0)
      {
        ticksremaining = calc_keepout(&left_bouncepos, &left_endpos);
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
          if (left_keepout_top <= TOPBAR_H + PADDLE_H)
          {
            // The ball is near the top so make sure it ends up right below it
            //DEBUG(putstring_nl("at the top"));
            left_dest = left_keepout_bot + 1;
          }
          else if (left_keepout_bot >= SCREEN_H - BOTBAR_H - PADDLE_H - 1)
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
        }
      }
      else
      {
        ticksremaining--;
      }

      // Draw the keepout area (for debugging)
      //glcdRectangle(LEFTPADDLE_X, left_keepout_top, PADDLE_W,
      //  left_keepout_bot - left_keepout_top);

      /*if (DEBUGGING)
      {
        putstring("\n\rdest dist: "); uart_putw_dec(ABS(leftpaddle_y - left_dest));
        putstring("\n\rtix: "); uart_putw_dec(ticksremaining);
        putstring("\n\rmax travel: "); uart_putw_dec(ticksremaining * MAX_PADDLE_SPEED);
        putstring_nl("");
      }*/

      // If we have just enough time, move the paddle
      leftpaddle_y = paddle_pos(left_dest, leftpaddle_y);
    }
  }

  // Make sure the paddles dont hit the top or bottom
  if (leftpaddle_y < TOPBAR_H + 1)
    leftpaddle_y = TOPBAR_H + 1;
  if (rightpaddle_y < TOPBAR_H + 1)
    rightpaddle_y = TOPBAR_H + 1;

  if (leftpaddle_y > SCREEN_H - PADDLE_H - BOTBAR_H - 1)
    leftpaddle_y = SCREEN_H - PADDLE_H - BOTBAR_H - 1;
  if (rightpaddle_y > SCREEN_H - PADDLE_H - BOTBAR_H - 1)
    rightpaddle_y = SCREEN_H - PADDLE_H - BOTBAR_H - 1;
}

//
// Function: update_alarm
//
// Prepare updates in pong due to change in alarm switch
//
static void update_alarm(void)
{
  if (mcUpdAlarmSwitch == GLCD_TRUE)
  {
    if (mcAlarmSwitch == ALARM_SWITCH_ON)
    {
      // Switch is switched to on so we may have to show the alarm time. Only
      // do so if we're not initializing pong.
      if (mcClockInit == GLCD_FALSE)
      {
        DEBUGP("Score -> Alarm");
        mcU8Util2 = SCORE_MODE_ALARM;
        mcU8Util4 = SCORE_TIMEOUT;
        set_score();
      }
    }
    else
    {
      // Switch is switched to off so revert back to time mode
      if (mcU8Util2 != SCORE_MODE_TIME)
        DEBUGP("Score -> Time");
      mcU8Util2 = SCORE_MODE_TIME;
      mcU8Util4 = 0;
      set_score();
    }
  }
}
