//*****************************************************************************
// Filename : 'perftest.c'
// Title    :  Test suite code for MONOCHRON glcd graphics performance tests
//*****************************************************************************

//
// This module is not a clock but instead is a high level glcd graphics
// performance test suite to be run on Monochron clock hardware and in the
// Emuchron emulator.
// The main purpose of this test suite is to get insight in the performance of
// high and low level glcd graphics functions. The test suite is used to verify
// whether (perceived) performance improvements in these glcd graphics
// functions actually deliver or not, and whether new/optimized graphics code
// is worth any (substantial) increase in atmel glcd object size.
//
// When building Monochron firmware, this module should be the only 'clock' in
// the Monochron[] array as it is designed to run indefinitely. Again, this is
// a test tool and not a functional clock. On Monochron, once the cycle()
// method of this 'clock' is called, it will never return control to main().
// In contrast with Monochron, in the emulator at the root level in a module,
// a 'q' keypress will exit the test suite and returns to the mchron caller
// function. In most cases this will be the mchron command prompt.
//
// The code also runs in the Emuchron emulator, but as the emulator runs on
// most likely an Intel/AMD class cpu, its speed performance results are
// irrelevant. However, information related to commands and bytes sent to and
// lcd data read from the controllers, and user calls to set the lcd cursor are
// useful metrics that are not retrieved while running the test on Monochron.
// Therefore, most insight in performance is gained by combining the statistics
// test results from the Emuchron emulator and test run time from the actual
// Monochron hardware.
//
// Running a test using the glut device, a test usually completes within a
// second. Running a test using the ncurses device a test will take much
// longer to complete but still less than on actual hardware. From a glcd and
// controller statistics point of view it does not matter which lcd device is
// used. It is therefor recommended to use the glut device only since it runs
// in its own thread and is therefor so much faster than the ncurses device.
//
// Note that this module requires the analog clock module to build as its
// functionality is used by a test in the glcdLine suite.
//
// The following high level glcd graphics functions are tested:
// - glcdCircle2
// - glcdDot
// - glcdLine
// - glcdFillCircle2 (specific use of glcdFillRectangle2)
// - glcdFillRectangle2
// - glcdPutStr3
// - glcdPutStr3v
// - glcdPutStr
// - glcdRectangle (specific use of glcdFillRectangle2)
//
// A test suite consists of one or more tests. A single test consists of
// generating many function calls to the function to be tested.
// The user interface to a test suite is split-up into the following elements:
// Part 1: Main entry for a test suite.
// - Press a button to enter the test suite or skip to the next suite (that
//   may be a restart at the first suite).
// Part 2: The following steps are repeated for each test in a test suite:
// - Press a button to start or skip the test.
// - Upon test start, sync on current time.
// - Generate many function calls to the glcd function. The test itself should
//   last about two minutes on an actual Monochron clock. Keep track of
//   relevant test statistics.
// - A test can be interrupted by a button press or a keyboard press.
// - Upon completing/aborting a test sync on current time and present test
//   statistics.
// - Press a button to rerun the test or continue with the next test.
// - When all tests are completed exit the current suite and continue with the
//   next suite, or restart at the first suite.
//
// WARNING: The code in this module bypasses the defined Monochron clock plugin
// framework for the greater good of providing a proper user interface and
// obtaining proper test results.
// Code in this module should not be replicated into clock modules that rely on
// the stability of the defined clock plugin framework. It's your choice.
//

#include <math.h>
#ifdef EMULIN
#include <stdio.h>
#include "../emulator/stub.h"
#include "../emulator/controller.h"
#include "../emulator/mchronutil.h"
#else
#include "../util.h"
#endif
#include "../monomain.h"
#include "../buttons.h"
#include "../ks0108.h"
#include "../glcd.h"
#include "../anim.h"
#include "perftest.h"
#include "analog.h"

// Refer to appendix B in Emuchron_Manual.pdf [support].
// The test loop numbers below have been recalibrated several times.
// Last calibration: Emuchron v5.0 using avr-gcc 5.4.0 (Debian 10).
// The test loop numbers make every test run on Monochron hardware complete in
// about 2 minutes.
#define PERF_DOT_1		70
#define PERF_DOT_2		84
#define PERF_LINE_1		11897
#define PERF_LINE_2		30
#define PERF_CIRCLE2_1		461
#define PERF_CIRCLE2_2		897
#define PERF_FILLCIRCLE2_1	314
#define PERF_FILLCIRCLE2_2	1010
#define PERF_RECTANGLE2_1	942
#define PERF_RECTANGLE2_2	600
#define PERF_FILLRECTANGLE2_1	1332
#define PERF_FILLRECTANGLE2_2	6890
#define PERF_FILLRECTANGLE2_3	4172
#define PERF_FILLRECTANGLE2_4	4046
#define PERF_PUTSTR3_1		1206
#define PERF_PUTSTR3_2		2366
#define PERF_PUTSTR3_3		1124
#define PERF_PUTSTR3_4		2367
#define PERF_PUTSTR3_5		2185
#define PERF_PUTSTR3V_1		953
#define PERF_PUTSTR3V_2		2313
#define PERF_PUTSTR3V_3		1115
#define PERF_PUTSTR3V_4		2692
#define PERF_PUTSTR_1		6010

// Defines on button press action flavors
#define PERF_WAIT_CONTINUE	0
#define PERF_WAIT_ENTER_SKIP	1
#define PERF_WAIT_START_SKIP	2
#define PERF_WAIT_RESTART_END	3

// Structure defining the admin data for test statistics
typedef struct
{
  char *text;
  u08 testId;
  u08 startSec;
  u08 startMin;
  u08 startHour;
  u08 endSec;
  u08 endMin;
  u08 endHour;
  u16 loopsDone;
  long elementsDrawn;
} testStats_t;

// Functional date/time/init variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;

// Functional alarm variables
extern volatile uint8_t mcAlarmSwitch;
extern volatile uint8_t mcAlarming;
extern volatile uint8_t mcUpdAlarmSwitch;

// Functional color variables
extern volatile uint8_t mcBgColor, mcFgColor;

// The internal RTC clock time and button press indicators
extern volatile rtcDateTime_t rtcDateTimeNext;
extern volatile uint8_t rtcTimeEvent;
extern volatile uint8_t btnPressed;

// Generic test utility function prototypes
static u08 perfButtonGet(void);
static u08 perfButtonWait(u08 type);
static void perfLongValToStr(long value, char valString[]);
static u08 perfSuiteWelcome(char *label);
static void perfTestBegin(void);
static u08 perfTestEnd(u08 interruptTest);
static u08 perfTestInit(char *label, u08 testId);
static void perfTestTimeInit(void);
static u08 perfTestTimeNext(void);
static void perfTextInit(u08 length);
static void perfTextToggle(void);

// The actual performance test function prototypes
static u08 perfTestCircle2(void);
static u08 perfTestDot(void);
static u08 perfTestFillCircle2(void);
static u08 perfTestFillRectangle2(void);
static u08 perfTestLine(void);
static u08 perfTestPutStr3(void);
static u08 perfTestPutStr3v(void);
static u08 perfTestPutStr(void);
static u08 perfTestRectangle(void);

// Runtime environment for the performance test
static testStats_t testStats;

// Text strings for glcdPutStr/glcdPutStr3/glcdPutStr3v tests
static char textLineA[33];
static char textLineY[33];
static char *textLine;

// Time counter for glcdLine-01
static u16 secCount;

//
// Function: perfCycle
//
// Run the performance test indefinitely
//
void perfCycle(void)
{
#ifdef EMULIN
  int myKbMode = KB_MODE_LINE;

  // In emulator switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);
#endif

  // Repeat forever
  while (1)
  {
    if (perfTestDot() == GLCD_TRUE)
      break;
    if (perfTestLine() == GLCD_TRUE)
      break;
    if (perfTestCircle2() == GLCD_TRUE)
      break;
    if (perfTestFillCircle2() == GLCD_TRUE)
      break;
    if (perfTestRectangle() == GLCD_TRUE)
      break;
    if (perfTestFillRectangle2() == GLCD_TRUE)
      break;
    if (perfTestPutStr3() == GLCD_TRUE)
      break;
    if (perfTestPutStr3v() == GLCD_TRUE)
      break;
    if (perfTestPutStr() == GLCD_TRUE)
      break;
  }

#ifdef EMULIN
  // This only happens in the emulator
  glcdClearScreen(mcBgColor);
  glcdPutStr2(1, 58, FONT_5X5P, "quit performance test", mcFgColor);

  // Return to line mode if needed
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_LINE);
#endif
}

//
// Function: perfInit
//
// Initialize the lcd display for the performance test suite
//
void perfInit(u08 mode)
{
  DEBUGP("Init Perftest");

  // Give welcome screen
  glcdPutStr2(1, 1, FONT_5X5P, "monochron glcd performance test", mcFgColor);
#ifdef EMULIN
  printf("\nTo exit performance test clock press 'q' on any main test suite prompt\n\n");
#endif

  // Wait for button press
  perfButtonWait(PERF_WAIT_CONTINUE);
}

//
// Function: perfTestCircle2
//
// Performance test of glcdCircle2()
//
static u08 perfTestCircle2(void)
{
  u08 button;
  u08 interruptTest;
  int i,j;
  int counter = 0;
  u08 x,y;

  // Give test suite welcome screen
  button = perfSuiteWelcome("glcdCircle2");
  if (button == 'q')
    return GLCD_TRUE;
  else if (button != BTN_PLUS)
    return GLCD_FALSE;

  // Test 1: Non-overlapping circles, each with different draw types.
  button = perfTestInit("glcdCircle2", 1);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Paint circles with various radius values and paint options
    interruptTest = GLCD_FALSE;
    perfTestBegin();
    for (i = 0; i < PERF_CIRCLE2_1; i++)
    {
      // Do the actual paint
      for (j = 0; j < 32; j++)
      {
        glcdCircle2(64, 32, j, counter % 3, mcFgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
        counter++;
      }

      // Undo paint
      counter = counter - 32;
      for (j = 0; j < 32; j++)
      {
        glcdCircle2(64, 32, j, counter % 3, mcBgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
        counter++;
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + j * 2;

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 2: Non-overlapping small circles, with remove redraw in phase 2.
  // The circles are identical to the ones drawn in puzzle.c, allowing a
  // good real-life measurement of draw optimizations.
  button = perfTestInit("glcdCircle2", 2);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Paint full circles with same radius values at different locations
    interruptTest = GLCD_FALSE;
    perfTestBegin();
    for (i = 0; i < PERF_CIRCLE2_2; i++)
    {
      for (x = 9; x < 120; x = x + 12)
      {
        // Paint small circles
        for (y = 8; y < 58; y = y + 12)
          glcdCircle2(x, y, 5, 0, mcFgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      for (x = 9; x < 120; x = x + 12)
      {
        // Clear small circles
        for (y = 8; y < 58; y = y + 12)
          glcdCircle2(x, y, 5, 0, mcBgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 10 * 5 * 2;

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  return GLCD_FALSE;
}

//
// Function: perfTestDot
//
// Performance test of glcdDot()
//
static u08 perfTestDot(void)
{
  u08 button;
  u08 interruptTest;
  int i;
  u08 j,x,y;

  // Give test suite welcome screen
  button = perfSuiteWelcome("glcdDot");
  if (button == 'q')
    return GLCD_TRUE;
  else if (button != BTN_PLUS)
    return GLCD_FALSE;

  // Test 1: Paint dots where each dot inverts the current color.
  // Will have a 100% lcd byte efficiency.
  button = perfTestInit("glcdDot", 1);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Fill screen with dots with a 100% replace rate
    interruptTest = GLCD_FALSE;
    perfTestBegin();
    for (i = 0; i < PERF_DOT_1; i++)
    {
      // Paint the dots
      for (j = 0; j < 8; j++)
      {
        for (x = 0; x < GLCD_XPIXELS; x++)
        {
          for (y = j; y < GLCD_YPIXELS; y = y + 8)
            glcdDot(x, y, mcFgColor);
#ifdef EMULIN
          ctrlLcdFlush();
#endif
        }
      }

      // Clear the dots
      for (j = 0; j < 8; j++)
      {
        for (x = 0; x < GLCD_XPIXELS; x++)
        {
          for (y = j; y < GLCD_YPIXELS; y = y + 8)
            glcdDot(x, y, mcBgColor);
#ifdef EMULIN
          ctrlLcdFlush();
#endif
        }
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn +
        2 * GLCD_XPIXELS * GLCD_YPIXELS;

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 2: Paint dots where, on average, each dot is inverted once in
  // every two update cycles.
  button = perfTestInit("glcdDot", 2);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Fill screen with dots with a 50% replace rate
    interruptTest = GLCD_FALSE;
    perfTestBegin();
    for (i = 0; i < PERF_DOT_2; i++)
    {
      // Paint the dots
      for (j = 0; j < 8; j++)
      {
        for (x = 0; x < GLCD_XPIXELS; x++)
        {
          for (y = j; y < GLCD_YPIXELS; y = y + 8)
            glcdDot(x, y, (i + 1) & 0x1);
#ifdef EMULIN
          ctrlLcdFlush();
#endif
        }
      }

      // Repaint the dots
      for (j = 0; j < 8; j++)
      {
        for (x = 0; x < GLCD_XPIXELS; x++)
        {
          for (y = j; y < GLCD_YPIXELS; y = y + 8)
            glcdDot(x, y, (i + 1) & 0x1);
#ifdef EMULIN
          ctrlLcdFlush();
#endif
        }
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn +
        2 * GLCD_XPIXELS * GLCD_YPIXELS;

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  return GLCD_FALSE;
}

//
// Function: perfTestLine
//
// Performance test of glcdLine()
//
static u08 perfTestLine(void)
{
  u08 button;
  u08 interruptTest;
  u08 xA, yA, xB, yB;
  int i, j;

  // Give test suite welcome screen
  button = perfSuiteWelcome("glcdLine");
  if (button == 'q')
    return GLCD_TRUE;
  else if (button != BTN_PLUS)
    return GLCD_FALSE;

  // Test 1: Draw analog clock updates. This gives a real-life measurement
  // of draw optimizations.
  button = perfTestInit("glcdLine", 1);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);
    perfTestTimeInit();
    analogHmsInit(DRAW_INIT_FULL);
    mcUpdAlarmSwitch = GLCD_TRUE;
    mcAlarmSwitch = GLCD_TRUE;

    // Draw lines using the analog clock layout
    interruptTest = GLCD_FALSE;
    perfTestBegin();
    do
    {
      // Paint all generated seconds using lines in analog clock
      analogCycle();
#ifdef EMULIN
      ctrlLcdFlush();
#endif
      // Do statistics
      testStats.elementsDrawn = testStats.elementsDrawn + 3;

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
      mcClockInit = GLCD_FALSE;
      mcUpdAlarmSwitch = GLCD_FALSE;

      // Do statistics
      testStats.loopsDone++;
    } while (perfTestTimeNext() == GLCD_FALSE);

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 2: Lines of varying length and draw angle.
  button = perfTestInit("glcdLine", 2);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Paint lines of varying length and draw angle
    interruptTest = GLCD_FALSE;
    perfTestBegin();
    for (i = 0; i < PERF_LINE_2; i++)
    {
      for (j = 0; j < 30 * 29; j = j + 1)
      {
        // Get begin and end points of line
        xA = (u08)(sin(2 * M_PI / 30 * (j % 30)) * 30 + 64);
        yA = (u08)(-cos(2 * M_PI / 30 * (j % 30)) * 30 + 32);
        xB = (u08)(sin(2 * M_PI / 29 * (j % 29)) * 30 + 64);
        yB = (u08)(-cos(2 * M_PI / 29 * (j % 29)) * 30 + 32);

        // Draw and remove the line
        glcdLine(xA, yA, xB, yB, mcFgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
        glcdLine(xA, yA, xB, yB, mcBgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + j * 2;

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  return GLCD_FALSE;
}

//
// Function: perfTestFillCircle2
//
// Performance test of glcdFillCircle2()
//
static u08 perfTestFillCircle2(void)
{
  u08 button;
  u08 interruptTest;
  u08 pattern;
  u08 color;
  u08 x, y;
  int i,j;
  int counter = 0;

  // Give test suite welcome screen
  button = perfSuiteWelcome("glcdFillCircle2");
  if (button == 'q')
    return GLCD_TRUE;
  else if (button != BTN_PLUS)
    return GLCD_FALSE;

  // Test 1: Overlapping filled circles, each with different fill types.
  button = perfTestInit("glcdFillCircle2", 1);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Paint circles with various radius values and paint options
    interruptTest = GLCD_FALSE;
    perfTestBegin();
    for (i = 0; i < PERF_FILLCIRCLE2_1; i++)
    {
      // Do the actual paint
      for (j = 0; j < 32; j++)
      {
        // Filltype inverse is not supported so skip that one
        if (counter % 6 == 4)
          counter++;
        glcdFillCircle2(64, 32, j, counter % 6, (j + counter) % 2);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
        counter++;
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + j;

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 2: Non-overlapping small filled circles.
  // The circles are identical to the ones drawn in puzzle.c, allowing a
  // good real-life measurement of draw optimizations.
  button = perfTestInit("glcdFillCircle2", 2);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Paint filled circles with same radius values at different locations
    interruptTest = GLCD_FALSE;
    pattern = 0;
    color = mcFgColor;
    perfTestBegin();
    for (i = 0; i < PERF_FILLCIRCLE2_2; i++)
    {
      for (x = 9; x < 120; x = x + 12)
      {
        // Paint small circles
        for (y = 8; y < 58; y = y + 12)
          glcdFillCircle2(x, y, 5, pattern, color);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 10 * 5;

      // Set draw parameters for next iteration
      if (pattern == 3)
      {
        // Skip pattern inverse (as it is not supported) and pattern clear
        // and restart. However, at restarting swap draw color.
        pattern = 0;
        if (color == mcFgColor)
          color = mcBgColor;
        else
          color = mcFgColor;
      }
      else
      {
        pattern++;
      }

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  return GLCD_FALSE;
}

//
// Function: perfTestFillRectangle2
//
// Performance test of glcdFillRectangle2()
//
static u08 perfTestFillRectangle2(void)
{
  u08 button;
  u08 interruptTest;
  int i,x,y;
  u08 dx,dy;

  // Give test suite welcome screen
  button = perfSuiteWelcome("glcdFillRectangle2");
  if (button == 'q')
    return GLCD_TRUE;
  else if (button != BTN_PLUS)
    return GLCD_FALSE;

  // Test 1: Replacing filled rectangles of varying small size, each with
  // different fill types.
  // It is the 'c' implementation of the rectangle5.txt test script with a
  // twist on paint color that varies per test cycle.
  button = perfTestInit("glcdFillRectangle2", 1);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);
    glcdRectangle(3, 6, 122, 57, mcFgColor);

    // Paint rectangles of varying size and fill options
    interruptTest = GLCD_FALSE;
    perfTestBegin();
    for (i = 0; i < PERF_FILLRECTANGLE2_1; i++)
    {
      dx = 1;
      // Vary on x axis
      for (x = 0; x < 14; x++)
      {
        dy = 1;
        // Vary on y axis
        for (y = 0; y < 9; y++)
        {
          glcdFillRectangle2(x + dx + 4, y + dy + 7, x + 1, y + 1, (x + y) % 3,
            (x + y + i) % 6, i % 2);
          dy = y + dy + 1;
#ifdef EMULIN
          ctrlLcdFlush();
#endif
        }
        dx = x + dx + 1;
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + x * y;

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 2: Painting large filled rectangles where a first paints a subset
  // area of a second one.
  button = perfTestInit("glcdFillRectangle2", 2);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Paint overlapping filled rectangles of varying fill options
    interruptTest = GLCD_FALSE;
    perfTestBegin();
    x = 0;
    y = 0;
    for (i = 0; i < PERF_FILLRECTANGLE2_2; i++)
    {
      // Swap fill values 3 and 5 and ignore 4
      if (x % 6 == 3)
      {
        y = 5;
      }
      else if (x % 6 == 4)
      {
        x++;
        y = 3;
      }
      else
      {
        y = x % 6;
      }
      glcdFillRectangle2(4, 4, 50, 35, ALIGN_AUTO, y, ((i / 5) + 1) & 0x1);
      glcdFillRectangle2(27, 17, 50, 45, ALIGN_AUTO, y, ((i / 5) + 1) & 0x1);
#ifdef EMULIN
      ctrlLcdFlush();
#endif
      x++;

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 2;

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 3: Painting large filled overlapping rectangles.
  button = perfTestInit("glcdFillRectangle2", 3);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Paint overlapping filled rectangles of varying fill options
    interruptTest = GLCD_FALSE;
    perfTestBegin();
    x = 0;
    y = 0;
    for (i = 0; i < PERF_FILLRECTANGLE2_3; i++)
    {
      // Swap fill values 3 and 5 and ignore 4
      if (x % 6 == 3)
      {
        y = 5;
      }
      else if (x % 6 == 4)
      {
        x++;
        y = 3;
      }
      else
      {
        y = x % 6;
      }
      glcdFillRectangle2(1, 1, 126, 60, i % 3, y, ((i / 5) + 1) & 0x1);
#ifdef EMULIN
      ctrlLcdFlush();
#endif
      x++;

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 1;

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 4: Painting large filled overlapping rectangles.
  // Only use HALF/THIRD fill types to test specific draw logic.
  button = perfTestInit("glcdFillRectangle2", 4);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Paint overlapping filled rectangles of varying fill options
    interruptTest = GLCD_FALSE;
    perfTestBegin();
    x = 0;
    y = 0;
    for (i = 0; i < PERF_FILLRECTANGLE2_4; i++)
    {
      glcdFillRectangle2(1, 1, 126, 60, (i * 5) % 3, i % 3 + 1, i & 0x1);
#ifdef EMULIN
      ctrlLcdFlush();
#endif

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 1;

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  return GLCD_FALSE;
}

//
// Function: perfTestPutStr3
//
// Performance test of glcdPutStr3()
//
static u08 perfTestPutStr3(void)
{
  u08 button;
  u08 interruptTest;
  int i;
  u08 y;

  // Give test suite welcome screen
  button = perfSuiteWelcome("glcdPutStr3");
  if (button == 'q')
    return GLCD_TRUE;
  else if (button != BTN_PLUS)
    return GLCD_FALSE;

  // Test 1: Draw text lines crossing a y-pixel byte. This is the most common
  // use for this function. Use the 5x7n font.
  button = perfTestInit("glcdPutStr3", 1);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Fill screen text strings crossing y-pixel bytes
    interruptTest = GLCD_FALSE;
    perfTextInit(21);
    perfTestBegin();
    for (i = 0; i < PERF_PUTSTR3_1; i++)
    {
      // Paint strings
      for (y = 3; y < GLCD_YPIXELS - 8; y = y + 8)
      {
        glcdPutStr3(1, y, FONT_5X7N, textLine, 1, 1, mcFgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 7;

      // Toggle text string
      perfTextToggle();

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 2: Draw text lines with font scaling, causing y-pixel byte crossings
  // and full lcd bytes to be written. Use the 5x7n font.
  button = perfTestInit("glcdPutStr3", 2);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Fill screen text strings crossing y-pixel bytes
    interruptTest = GLCD_FALSE;
    perfTextInit(7);
    perfTestBegin();
    for (i = 0; i < PERF_PUTSTR3_2; i++)
    {
      // Paint strings
      for (y = 0; y < GLCD_YPIXELS - 21; y = y + 21)
      {
        glcdPutStr3(2, y, FONT_5X7N, textLine, 3, 3, mcFgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 3;

      // Toggle text string
      perfTextToggle();

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 3: Draw text lines crossing a y-pixel byte. Use the 5x5p font.
  button = perfTestInit("glcdPutStr3", 3);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Fill screen text strings crossing y-pixel bytes
    interruptTest = GLCD_FALSE;
    perfTextInit(31);
    perfTestBegin();
    for (i = 0; i < PERF_PUTSTR3_3; i++)
    {
      // Paint strings
      for (y = 1; y < GLCD_YPIXELS - 6; y = y + 6)
      {
        glcdPutStr3(2, y, FONT_5X5P, textLine, 1, 1, mcFgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 10;

      // Toggle text string
      perfTextToggle();

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 4: Draw text lines with font scaling, causing y-pixel byte crossings
  // and full lcd bytes to be written. Use the 5x5p font.
  button = perfTestInit("glcdPutStr3", 4);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Fill screen text strings crossing y-pixel bytes
    interruptTest = GLCD_FALSE;
    perfTextInit(10);
    perfTestBegin();
    for (i = 0; i < PERF_PUTSTR3_4; i++)
    {
      // Paint strings
      for (y = 1; y < GLCD_YPIXELS - 15; y = y + 15)
      {
        glcdPutStr3(4, y, FONT_5X5P, textLine, 3, 3, mcFgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 4;

      // Toggle text string
      perfTextToggle();

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 5: Draw text lines fitting in a single y-pixel byte.
  // Use the 5x7n font.
  button = perfTestInit("glcdPutStr3", 5);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Fill screen text strings that fit in a y-pixel byte
    interruptTest = GLCD_FALSE;
    perfTextInit(21);
    perfTestBegin();
    for (i = 0; i < PERF_PUTSTR3_5; i++)
    {
      // Paint strings
      for (y = 0; y <= GLCD_YPIXELS - 8; y = y + 8)
      {
        glcdPutStr3(1, y, FONT_5X7N, textLine, 1, 1, mcFgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 7;

      // Toggle text string
      perfTextToggle();

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  return GLCD_FALSE;
}

//
// Function: perfTestPutStr3v
//
// Performance test of glcdPutStr3v()
//
static u08 perfTestPutStr3v(void)
{
  u08 button;
  u08 interruptTest;
  int i;
  u08 x;

  // Give test suite welcome screen
  button = perfSuiteWelcome("glcdPutStr3v");
  if (button == 'q')
    return GLCD_TRUE;
  else if (button != BTN_PLUS)
    return GLCD_FALSE;

  // Test 1: Draw text lines bottom-up without font scaling.
  // This is the most common use for this function. Use font 5x5p.
  button = perfTestInit("glcdPutStr3v", 1);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Fill screen text strings crossing y-pixel bytes
    interruptTest = GLCD_FALSE;
    perfTextInit(15);
    perfTestBegin();
    for (i = 0; i < PERF_PUTSTR3V_1; i++)
    {
      // Paint strings
      for (x = 1; x < GLCD_XPIXELS - 6; x = x + 6)
      {
        glcdPutStr3v(x, 61, FONT_5X5P, ORI_VERTICAL_BU, textLine, 1, 1,
          mcFgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 21;

      // Toggle text string
      perfTextToggle();

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 2: Draw text lines bottom-up with font scaling.
  // Use font 5x7n.
  button = perfTestInit("glcdPutStr3v", 2);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Fill screen text strings crossing y-pixel bytes
    interruptTest = GLCD_FALSE;
    perfTextInit(5);
    perfTestBegin();
    for (i = 0; i < PERF_PUTSTR3V_2; i++)
    {
      // Paint strings
      for (x = 1; x < GLCD_XPIXELS - 21; x = x + 21)
      {
        glcdPutStr3v(x, 60, FONT_5X7N, ORI_VERTICAL_BU, textLine, 3, 2,
          mcFgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 6;

      // Toggle text string
      perfTextToggle();

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 3: Draw text lines top-down without font scaling.
  // This is the most common use for this function. Use font 5x7n.
  button = perfTestInit("glcdPutStr3v", 3);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Fill screen text strings crossing y-pixel bytes
    interruptTest = GLCD_FALSE;
    perfTextInit(10);
    perfTestBegin();
    for (i = 0; i < PERF_PUTSTR3V_3; i++)
    {
      // Paint strings
      for (x = 7; x < GLCD_XPIXELS; x = x + 9)
      {
        glcdPutStr3v(x, 2, FONT_5X7N, ORI_VERTICAL_TD, textLine, 1, 1,
          mcFgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 14;

      // Toggle text string
      perfTextToggle();

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 4: Draw text lines top-down with font scaling.
  // Use font 5x5p.
  button = perfTestInit("glcdPutStr3v", 4);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Fill screen with text strings crossing y-pixel bytes
    interruptTest = GLCD_FALSE;
    perfTextInit(7);
    perfTestBegin();
    for (i = 0; i < PERF_PUTSTR3V_4; i++)
    {
      // Paint strings
      for (x = 17; x < GLCD_XPIXELS; x = x + 18)
      {
        glcdPutStr3v(x, 4, FONT_5X5P, ORI_VERTICAL_TD, textLine, 3, 2,
          mcFgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 7;

      // Toggle text string
      perfTextToggle();

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  return GLCD_FALSE;
}

//
// Function: perfTestPutStr
//
// Performance test of glcdPutStr()
//
static u08 perfTestPutStr(void)
{
  u08 button;
  u08 interruptTest;
  int i;
  u08 y;

  // Give test suite welcome screen
  button = perfSuiteWelcome("glcdPutStr");
  if (button == 'q')
    return GLCD_TRUE;
  else if (button != BTN_PLUS)
    return GLCD_FALSE;

  // Test 1: Draw text lines, being the most common use for this function.
  button = perfTestInit("glcdPutStr", 1);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Fill screen text strings
    interruptTest = GLCD_FALSE;
    perfTextInit(21);
    perfTestBegin();
    for (i = 0; i < PERF_PUTSTR_1; i++)
    {
      // Paint strings
      for (y = 0; y < GLCD_CONTROLLER_YPAGES; y++)
      {
        glcdSetAddress(1, y);
        glcdPutStr(textLine, mcFgColor);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 8;

      // Toggle text string
      perfTextToggle();

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  return GLCD_FALSE;
}

//
// Function: perfTestRectangle
//
// Performance test of glcdRectangle()
//
static u08 perfTestRectangle(void)
{
  u08 button;
  u08 interruptTest;
  int i,x,y;
  u08 dx,dy;

  // Give test suite welcome screen
  button = perfSuiteWelcome("glcdRectangle");
  if (button == 'q')
    return GLCD_TRUE;
  else if (button != BTN_PLUS)
    return GLCD_FALSE;

  // Test 1: Painting small rectangles of varying size.
  // It is the 'c' implementation of the rectangle1.txt test script.
  button = perfTestInit("glcdRectangle", 1);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);
    glcdRectangle(3, 6, 122, 57, mcFgColor);

    // Paint rectangles of varying size and fill options
    interruptTest = GLCD_FALSE;
    perfTestBegin();
    for (i = 0; i < PERF_RECTANGLE2_1; i++)
    {
      dx = 1;
      // Vary on x axis
      for (x = 0; x < 14; x++)
      {
        dy = 1;
        // Vary on y axis
        for (y = 0; y < 9; y++)
        {
          glcdRectangle(x + dx + 4, y + dy + 7, x + 1, y + 1, i % 2);
          dy = y + dy + 1;
#ifdef EMULIN
          ctrlLcdFlush();
#endif
        }
        dx = x + dx + 1;
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + x * y;

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  // Test 2: Painting larger rectangles
  button = perfTestInit("glcdRectangle", 2);
  while (button == BTN_PLUS)
  {
    // Prepare display for test
    glcdClearScreen(mcBgColor);

    // Paint rectangles of varying fill options
    interruptTest = GLCD_FALSE;
    perfTestBegin();
    x = 0;
    y = 0;
    for (i = 0; i < PERF_RECTANGLE2_2; i++)
    {
      for (y = 1; y < 64; y = y + 2)
      {
        glcdRectangle(64 - y, 32 - y / 2, y * 2, y, i & 0x1);
#ifdef EMULIN
        ctrlLcdFlush();
#endif
      }

      // Do statistics
      testStats.loopsDone++;
      testStats.elementsDrawn = testStats.elementsDrawn + 32;

      // Check for keypress interrupt
      button = perfButtonGet();
      if (button != 0)
      {
        interruptTest = GLCD_TRUE;
        break;
      }
    }

    // End test and report statistics
    button = perfTestEnd(interruptTest);
  }

  return GLCD_FALSE;
}

//
// Function: perfButtonGet
//
// Get button when pressed
//
static u08 perfButtonGet(void)
{
#ifndef EMULIN
  u08 button;

  // Get and clear (if any) button
  button = btnPressed;
  btnPressed = BTN_NONE;
  return button;
#else
  // Accept any keypress when pressed
  return (u08)kbKeypressScan(GLCD_FALSE);
#endif
}

//
// Function: perfButtonWait
//
// Wait for any button to be pressed
//
static u08 perfButtonWait(u08 type)
{
  u08 button = 0;

  // Give wait message
  glcdFillRectangle2(0, 58, 127, 5, ALIGN_TOP, FILL_BLANK, mcFgColor);
  if (type == PERF_WAIT_CONTINUE)
    glcdPutStr2(1, 58, FONT_5X5P, "press button to continue", mcFgColor);
  else if (type == PERF_WAIT_ENTER_SKIP)
    glcdPutStr2(1, 58, FONT_5X5P, "+ = enter, set/menu = skip", mcFgColor);
  else if (type == PERF_WAIT_START_SKIP)
    glcdPutStr2(1, 58, FONT_5X5P, "+ = start, set/menu = skip", mcFgColor);
  else // type == PERF_WAIT_RESTART_END
    glcdPutStr2(1, 58, FONT_5X5P, "+ = restart, set/menu = end", mcFgColor);
#ifdef EMULIN
  ctrlLcdFlush();
#endif

  // Clear any button pressed and wait for button
  btnPressed = BTN_NONE;

#ifndef EMULIN
  // Get any button from Monochron
  while (btnPressed == BTN_NONE);
  button = btnPressed;
  btnPressed = BTN_NONE;
#else
  // Get +,s,m,q, others default to MENU button
  char ch = waitKeypress(GLCD_FALSE);
  if (ch >= 'A' && ch <= 'Z')
    ch = ch - 'A' + 'a';
  if (ch == '+')
    button = BTN_PLUS;
  else if (ch == 's')
    button = BTN_SET;
  else if (ch == 'm')
    button = BTN_MENU;
  else if (ch == 'q')
    button = 'q';
  else
    button = BTN_MENU;
#endif

  return button;
}

//
// Function: perfLongValToStr
//
// Make a string out of a positive long integer value
//
static void perfLongValToStr(long value, char valString[])
{
  u08 index = 0;
  u08 i;
  char temp;
  long valTemp = value;

  // Create string in reverse order
  while (valTemp > 0 || index == 0)
  {
    valString[index] = valTemp % 10 + '0';
    valTemp = valTemp / 10;
    index++;
  }
  valString[index] = '\0';

  // Reverse string
  for (i = 0; i < (index + 1) / 2; i++)
  {
    temp = valString[i];
    valString[i] = valString[index - i - 1];
    valString[index - i - 1] = temp;
  }
}

//
// Function: perfSuiteWelcome
//
// Provide welcome of test suite
//
static u08 perfSuiteWelcome(char *label)
{
  u08 length;

  // Give test suite welcome screen
  glcdClearScreen(mcBgColor);
  length = glcdPutStr2(1, 1, FONT_5X5P, "Test suite: ", mcFgColor);
  glcdPutStr2(length + 1, 1, FONT_5X5P, label, mcFgColor);

  // Wait for button press: continue or skip all tests
  // + = continue
  // s/m = skip
  // q = quit (emulator only)
  return perfButtonWait(PERF_WAIT_ENTER_SKIP);
}

//
// Function: perfTestBegin
//
// Clear previous test statistics and mark test start time
//
static void perfTestBegin(void)
{
#ifdef EMULIN
  // In case we're using glut, give the lcd device some time to catch up
  _delay_ms(250);
  // Reset glcd/controller statistics
  ctrlStatsReset(CTRL_STATS_GLCD | CTRL_STATS_CTRL);
#endif

  // Clear previous test statistics
  testStats.endSec = 0;
  testStats.endMin = 0;
  testStats.endHour = 0;
  testStats.loopsDone = 0;
  testStats.elementsDrawn = 0;

  // Resync time after which we'll wait for the next second to occur
  // for a more consistent duration measurement
  rtcMchronTimeInit();
  rtcTimeEvent = GLCD_FALSE;
#ifndef EMULIN
  while (rtcTimeEvent == GLCD_FALSE);
#else
  while (rtcTimeEvent == GLCD_FALSE)
  {
    stubDelay(25);
    monoTimer();
  }
#endif

  // Mark test start time
  testStats.startSec = rtcDateTimeNext.timeSec;
  testStats.startMin = rtcDateTimeNext.timeMin;
  testStats.startHour = rtcDateTimeNext.timeHour;
}

//
// Function: perfTestEnd
//
// Mark test end time and report final test statistics
//
static u08 perfTestEnd(u08 interruptTest)
{
  char number[20];
  u08 length;

  // Clear any button press
  btnPressed = BTN_NONE;

  // Mark test end time
  rtcMchronTimeInit();
  testStats.endSec = rtcDateTimeNext.timeSec;
  testStats.endMin = rtcDateTimeNext.timeMin;
  testStats.endHour = rtcDateTimeNext.timeHour;

#ifdef EMULIN
  // In case we're using glut, give the lcd device some time to catch up
  _delay_ms(250);

  // Give test end result and glcd/controller statistics
  printf("test   : %s - %02d\n", testStats.text, testStats.testId);
  if (interruptTest == GLCD_FALSE)
    printf("status : %s\n", "completed");
  else
    printf("status : %s\n", "aborted");
  ctrlStatsPrint(CTRL_STATS_GLCD | CTRL_STATS_CTRL);
#endif

  // Give test statistics screen
  glcdClearScreen(mcBgColor);
  glcdPutStr2(1, 1, FONT_5X5P, "test:", mcFgColor);
  length = glcdPutStr2(29, 1, FONT_5X5P, testStats.text, mcFgColor);
  length = length + glcdPutStr2(length + 29, 1, FONT_5X5P, " - ", mcFgColor);
  animValToStr(testStats.testId, number);
  glcdPutStr2(length + 29, 1, FONT_5X5P, number, mcFgColor);
  glcdPutStr2(1, 7, FONT_5X5P, "status:", mcFgColor);
  if (interruptTest == GLCD_FALSE)
    glcdPutStr2(29, 7, FONT_5X5P, "completed", mcFgColor);
  else
    glcdPutStr2(29, 7, FONT_5X5P, "aborted", mcFgColor);

  // Start time
  glcdPutStr2(1, 13, FONT_5X5P, "start:", mcFgColor);
  animValToStr(testStats.startHour, number);
  glcdPutStr2(29, 13, FONT_5X5P, number, mcFgColor);
  glcdPutStr2(37, 13, FONT_5X5P, ":", mcFgColor);
  animValToStr(testStats.startMin, number);
  glcdPutStr2(39, 13, FONT_5X5P, number, mcFgColor);
  glcdPutStr2(47, 13, FONT_5X5P, ":", mcFgColor);
  animValToStr(testStats.startSec, number);
  glcdPutStr2(49, 13, FONT_5X5P, number, mcFgColor);

  // End time
  glcdPutStr2(1, 19, FONT_5X5P, "end:", mcFgColor);
  animValToStr(testStats.endHour, number);
  glcdPutStr2(29, 19, FONT_5X5P, number, mcFgColor);
  glcdPutStr2(37, 19, FONT_5X5P, ":", mcFgColor);
  animValToStr(testStats.endMin, number);
  glcdPutStr2(39, 19, FONT_5X5P, number, mcFgColor);
  glcdPutStr2(47, 19, FONT_5X5P, ":", mcFgColor);
  animValToStr(testStats.endSec, number);
  glcdPutStr2(49, 19, FONT_5X5P, number, mcFgColor);

  // Cycles
  glcdPutStr2(1, 25, FONT_5X5P, "cycles:", mcFgColor);
  perfLongValToStr(testStats.loopsDone, number);
  glcdPutStr2(29, 25, FONT_5X5P, number, mcFgColor);

  // Elements drawn
  glcdPutStr2(1, 31, FONT_5X5P, "draws:", mcFgColor);
  perfLongValToStr(testStats.elementsDrawn, number);
  glcdPutStr2(29, 31, FONT_5X5P, number, mcFgColor);

  // Wait for button press
  return perfButtonWait(PERF_WAIT_RESTART_END);
}

//
// Function: perfTestInit
//
// Init test id and provide test prompt
//
static u08 perfTestInit(char *label, u08 testId)
{
  u08 length;
  char strTestId[3];

  // Init test id
  testStats.text = label;
  testStats.testId = testId;

  // Provide prompt to run or skip test
  glcdClearScreen(mcBgColor);
  length = glcdPutStr2(1, 1, FONT_5X5P, label, mcFgColor);
  length = length + glcdPutStr2(length + 1, 1, FONT_5X5P, " - ", mcFgColor);
  animValToStr(testId, strTestId);
  glcdPutStr2(length + 1, 1, FONT_5X5P, strTestId, mcFgColor);

  return perfButtonWait(PERF_WAIT_START_SKIP);
}

//
// Function: perfTestTimeInit
//
// Initialize functional Monochron time.
//
static void perfTestTimeInit(void)
{
  mcClockOldTS = mcClockNewTS = mcClockOldTM = mcClockNewTM = 0;
  mcClockOldTH = mcClockNewTH = 0;
  mcClockOldDD = mcClockNewDD = mcClockOldDM = mcClockNewDM = 1;
  mcClockOldDY = mcClockNewDY = 15;
  secCount = PERF_LINE_1;
  mcAlarming = GLCD_FALSE;
}

//
// Function: perfTestTimeNext
//
// Get next timestamp up to 1 hour after start
//
static u08 perfTestTimeNext(void)
{
  mcClockOldTS = mcClockNewTS;
  mcClockOldTM = mcClockNewTM;
  mcClockOldTH = mcClockNewTH;
  mcClockOldDD = mcClockNewDD;
  mcClockOldDM = mcClockNewDM;
  mcClockOldDY = mcClockNewDY;

  secCount--;
  if (secCount == 0)
    return GLCD_TRUE;

  if (mcClockNewTS != 59)
  {
    // Next second
    mcClockNewTS++;
  }
  else if (mcClockNewTM != 59)
  {
    // Next minute
    mcClockNewTS = 0;
    mcClockNewTM++;
  }
  else
  {
    // Next hour
    mcClockNewTS = 0;
    mcClockNewTM = 0;
    mcClockNewTH++;
  }

  return GLCD_FALSE;
}

//
// Function: perfTextInit
//
// Create text strings with 'A' and 'Y' characters.
// These characters are chosen as in the 5x5p font they both have width 3,
// which is more or less average.
// Also set the first text string to be used in a perf test.
//
static void perfTextInit(u08 length)
{
  u08 i;

  // Generate 'A' and 'Y' strings of requested length
  for (i = 0; i < length; i++)
  {
    textLineA[i] = 'A';
    textLineY[i] = 'Y';
  }
  textLineA[i] = '\0';
  textLineY[i] = '\0';

  // Initialise the first string to be used in a test
  textLine = textLineA;
}

//
// Function: perfTextToggle
//
// Toggle the text string to be used in a perf test.
//
static void perfTextToggle(void)
{
  if (textLine == textLineA)
    textLine = textLineY;
  else
    textLine = textLineA;
}
