//*****************************************************************************
// Filename : 'ratt.c'
// Title    : The main clock engine for MONOCHRON
//*****************************************************************************

#ifndef EMULIN
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <i2c.h>
#include "buttons.h"
#include "util.h"
#else
#include "emulator/stub.h"
#include "emulator/stubrefs.h"
#endif
#include "ratt.h"
#include "ks0108.h"
#include "glcd.h"
#include "config.h"
#include "anim.h"
#ifdef MARIO
#include "mariotune.h"
#endif

// The following variables are for internal use only to drive all
// mcVariable elements. They are not be used in any Monochron
// clock as their contents are considered unstable.
volatile uint8_t time_s, time_m, time_h;
volatile uint8_t date_d, date_m, date_y;
volatile uint8_t new_ts, new_tm, new_th;
volatile uint8_t new_dd, new_dm, new_dy;
volatile uint8_t old_m, old_h;
volatile uint8_t time_event = GLCD_FALSE;
volatile uint8_t displaymode = SHOW_TIME;
volatile uint8_t alarmOn, alarmSelect;
volatile uint8_t alarming = GLCD_FALSE;

// Indicates whether in the config menu something is busy writing
// to the LCD, thus interfering with the process to update the time
// in the config screen.
extern volatile uint8_t screenmutex;

// These store the current button states for all 3 buttons. We can 
// then query whether the buttons are pressed and released or pressed.
// This allows for 'high speed incrementing' when setting config values.
extern volatile uint8_t just_pressed;
extern volatile uint8_t buttonholdcounter;

// Configuration keypress timeout counter
extern volatile uint8_t timeoutcounter;

// How long we have been snoozing and alarming
uint16_t snoozeTimer = 0;
int16_t alarmTimer = 0;
volatile uint16_t animTicker, alarmTicker;

// Runtime data for two-tone or Mario alarm
#ifdef MARIO
uint16_t marioFreq = 0;
uint8_t marioIdx = 0;
uint8_t marioIdxEnd = 0;
uint8_t marioMasterIdx = (uint8_t)(sizeof(MarioMaster) - 2);
uint8_t marioPauze = GLCD_TRUE;
#else
uint8_t alarmTone = 0;
#endif

// The following global variables are for use in any Monochron clock.
// In a Monochron clock its contents are defined and stable.
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcAlarmH, mcAlarmM;
extern volatile uint8_t mcMchronClock;
extern volatile uint8_t mcCycleCounter;
extern volatile uint8_t mcFgColor;
extern volatile uint8_t mcBgColor;

// The static list of clocks supported in Monochron and its pool pointer
extern clockDriver_t monochron[];
extern clockDriver_t *mcClockPool;

// Time dividers
uint8_t t2divider1 = 0;
//uint8_t t2divider2 = 0;

// Local function prototypes
void rtcTimeInit(void);
void snoozeSet(void);

//
// Function: main
//
// The Monochron main() function. It initializes the Monochron environment
// and ends up in an infinite loop that processes button presses and
// switches between and updates Monochron clocks.
//
#ifdef EMULIN
int stubMain(void)
#else
int main(void)
#endif
{
  u08 doNextClock = GLCD_FALSE;

  // Check if we were reset
  MCUSR = 0;
  
  // Just in case we were reset inside of the glcd init function
  // which would happen if the lcd is not plugged in. The end result
  // of that is it will beep, pause, for as long as there is no lcd
  // plugged in.
  wdt_disable();

  // Init uart
  DEBUGP("*** UART");
  uart_init(BRRL_192);

  // Init piezo
  DEBUGP("*** Piezo");
  PIEZO_DDR |= _BV(PIEZO);

  // Init system real time clock
  DEBUGP("*** System clock");
  new_ts = 60;
  rtcTimeInit();

  // Init data saved in eeprom
  DEBUGP("*** EEPROM");
  init_eeprom();
  mcBgColor = eeprom_read_byte((uint8_t *)EE_BGCOLOR) % 2;
  mcFgColor = (mcBgColor == OFF ? ON : OFF);
  alarmSelect = eeprom_read_byte((uint8_t *)EE_ALARM_SELECT) % 4;
  alarmTimeGet(alarmSelect, &mcAlarmH, &mcAlarmM);
  
  // Init buttons
  DEBUGP("*** Buttons");
  buttonsInit();

  // Init based on alarm switch
  DEBUGP("*** Alarmstate");
  alarmOn = GLCD_FALSE;
  alarming = GLCD_FALSE;
  snoozeTimer = 0;
  alarmTimer = 0;
  alarmStateSet();

  // Setup 1-ms timer on timer0
  DEBUGP("*** 1-ms Timer");
  TCCR0A = _BV(WGM01);
  TCCR0B = _BV(CS01) | _BV(CS00);
  OCR0A = 125;
  TIMSK0 |= _BV(OCIE0A);

  // Turn backlight on
  DEBUGP("*** Backlight");
  DDRD |= _BV(3);
#ifndef BACKLIGHT_ADJUST
  PORTD |= _BV(3);
#else
  TCCR2A = _BV(COM2B1); // PWM output on pin D3
  TCCR2A |= _BV(WGM21) | _BV(WGM20); // fast PWM
  TCCR2B |= _BV(WGM22);
  OCR2A = OCR2A_VALUE;
  OCR2B = eeprom_read_byte((uint8_t *)EE_BRIGHT);
#endif
  DDRB |= _BV(5);
  beep(4000, 100);

  // Init LCD.
  // glcdInit locks and disables interrupts in one of its functions.
  // If the LCD is not plugged in, glcd will run forever. For good
  // reason, it would be desirable to know that the LCD is plugged in
  // and working correctly as a result. This is why we are using a
  // watch dog timer. The lcd should be initialized in way less than
  // 500 ms.
  DEBUGP("*** LCD");
  wdt_enable(WDTO_2S);
  glcdInit(mcBgColor);

  // Be friendly and give a welcome message
  DEBUGP("*** Welcome");
  animWelcome();

  // Init to display the first defined Monochron clock
  DEBUGP("*** Start initial clock");
  mchronTimeInit();
  mcClockPool = monochron;
  mcMchronClock = 0;
  displaymode = SHOW_TIME;
  animClockDraw(DRAW_INIT_FULL);
  DEBUGP("*** Init clock completed");

  // This is the main event loop handler that will run forever
  while (1)
  {
    // Set the duration of a single loop cycle
    animTicker = ANIMTICK_MS;

    // Check buttons to see if we have interaction stuff to deal with

    // First, when alarming while showing a clock, any button press will
    // make us (re)snooze. This rather crude method of button handling turns
    // out to be end-user friendly as it is simple and easy to comprehend.
    if (just_pressed && alarming == GLCD_TRUE && displaymode == SHOW_TIME)
    {
      snoozeSet();
      just_pressed = 0;
    }

    // At this stage potentially every button may be flagged as being
    // pressed. To avoid race conditions between buttons allow only a single
    // button press to be processed in a cycle and ignore the rest. The
    // latter will be achieved by clearing just_pressed when a button press
    // has been signalled.

    // In checking the buttons, the Menu button has highest priority as it
    // drives the config menu state-event machine and we don't want anything
    // else to interfere with that.
    if (just_pressed & BTTN_MENU)
    {
      just_pressed = 0;

      // The Menu button is pressed so initiate the state-event config menu
      // or make it navigate to its next menu item. The latter is done by
      // processing the just_pressed from within the menu to signal to
      // continue at the next menu item in the config menu, or it signal
      // its completion.
      menu_main();
      if (timeoutcounter == 0)
      {
        DEBUGP("Keypress timeout -> resume to clock");
      }

      // If the config menu is completely done, re-init both the time and a
      // clock. We need to re-init Monochron time since it is most likely
      // we've been in the config menu for several seconds.
      if (displaymode == SHOW_TIME)
      {
        mchronTimeInit();
        animClockDraw(DRAW_INIT_FULL);
      }
    }
    else // Handle the set or + button
    {
      // Check the Set button
      if (just_pressed & BTTN_SET)
      {
        if (animClockButton(just_pressed) == GLCD_FALSE)
        {
          // No button method has been defined for the active clock.
          // Default to the action set for the + button.
          doNextClock = GLCD_TRUE;
          DEBUGP("Set button dflt to +");
        }
        else
        {
          // Set button has been processed
          just_pressed = 0;
        }
      }
 
      // Check the + button and default set button action
      if ((just_pressed & BTTN_PLUS) || doNextClock == GLCD_TRUE)
      {
        u08 initType;
        u08 currMchronClock = mcMchronClock;

        // Select the next clock
        DEBUGP("Clock -> Next clock");
        initType = animClockNext();

        if (currMchronClock != mcMchronClock)
        {
          // We have a new clock to initialize
          animClockDraw(initType);
        }
        else
        {
          // There is only one clock configured
          animClockButton(just_pressed);
        }

        doNextClock = GLCD_FALSE;
        just_pressed = 0;
      }
    }

    // We're now done with button handling. If a Monochron clock is active
    // have it update itself based on time/alarm/init events and data set
    // in the global mcVariable variables.
    if (displaymode == SHOW_TIME)
    {
      // Set time event state for clock cycle event handler and execute it
      mcClockTimeEvent = time_event;
      animClockDraw(DRAW_CYCLE);
      if (mcClockTimeEvent == GLCD_TRUE)
      {
        // Clear the time event only when set
        DEBUGP("Clear time event");
        mcClockTimeEvent = GLCD_FALSE;
        time_event = GLCD_FALSE;
      }
    }

    // Get event(s) while waiting the remaining time of the loop cycle
#ifdef EMULIN
    if (stubGetEvent() == 'q')
      return 0;
#else
    while (animTicker);
    // Uncomment this to manually 'step' using a terminal keypress via FTDI
    //uart_getchar();
#endif

    // Admin on cycle counter
    mcCycleCounter++;
  }

  // We should never get here
  halt();
}

//
// 1 msec signal handler
//
// Used for handling audible alarm and switching between tones in audible
// alarm. As this is called every 1 msec try to keep its CPU footprint
// as small as possible.
//
#ifndef EMULIN
SIGNAL(TIMER0_COMPA_vect)
{
  // Countdown timers
  if (animTicker)
    animTicker--;
  if (buttonholdcounter)
    buttonholdcounter--;

  if (alarming == GLCD_TRUE && snoozeTimer == 0)
  {
    // We're alarming with sound
    if (alarmTicker == 0)
    {
#ifdef MARIO
      // Mario chiptune alarm
      if (marioIdx == marioIdxEnd && marioPauze == GLCD_TRUE)
      {
        // End of current tune line. Move to next line or continue at beginning.
        if (marioMasterIdx == (uint8_t)(sizeof(MarioMaster) - 2))
          marioMasterIdx = 0;
        else
          marioMasterIdx = marioMasterIdx + 2;

        marioIdx = pgm_read_byte(&MarioMaster[marioMasterIdx]);
        marioIdxEnd = marioIdx + pgm_read_byte(&MarioMaster[marioMasterIdx + 1]);
      }

      // Should we play a tone or a post-tone half beat pauze
      if (marioPauze == GLCD_TRUE)
      {
        // Last played a half beat pauze, so now we play a tone
        marioPauze = GLCD_FALSE;
        marioFreq = (uint16_t)(pgm_read_byte(&MarioTones[marioIdx])) *
          MAR_TONEFACTOR;
        alarmTicker = (uint16_t)(pgm_read_byte(&MarioBeats[marioIdx])) *
          MAR_TEMPO / MAR_BEATFACTOR;
      }
      else
      {
        // Last played a tone, so now we play a half beat pauze
        marioPauze = GLCD_TRUE;
        marioFreq = 0;
        alarmTicker = MAR_TEMPO / 2;
        // When done we move to next tone
        marioIdx++;
      }

      if (marioFreq == 0)
      {
        // Be silent
        TCCR1B = 0;
        // Turn off piezo
        PIEZO_PORT &= ~_BV(PIEZO);
      }
      else
      {
        // Make noise
        TCCR1A = 0;
        TCCR1B = _BV(WGM12) | _BV(CS10);
        TIMSK1 = _BV(TOIE1) | _BV(OCIE1A);
        // Set the frequency to use
        OCR1A = (F_CPU / marioFreq) / 2;
      }
#else
      // Two-tone alarm
      // Tone cycle period timeout: go to next one
      alarmTicker = ALARMTICK_MS;
      if (TCCR1B == 0)
      {
        // End of silent period: next one will do audio 
        TCCR1A = 0;
        TCCR1B = _BV(WGM12) | _BV(CS10); // CTC with fastest timer
        TIMSK1 = _BV(TOIE1) | _BV(OCIE1A);
        // Select the frequency to use
        if (alarmTone == 0)
          OCR1A = (F_CPU / ALARM_FREQ_1) / 2;
        else
          OCR1A = (F_CPU / ALARM_FREQ_2) / 2;
        // Toggle frequency for next audio cycle
        alarmTone = ~alarmTone;
      }
      else
      {
        // End of audio period: next one be silent
        TCCR1B = 0;
        // Turn off piezo
        PIEZO_PORT &= ~_BV(PIEZO);
      }
#endif
    }
    alarmTicker--;
  }
  else if (alarmTimer == -1)
  {
    // Respond to request to stop alarm due to alarm timeout
    TCCR1B = 0;
    // Turn off piezo
    PIEZO_PORT &= ~_BV(PIEZO);
    alarmTimer = 0;
#ifdef MARIO
    // On next audible alarm start at beginning of Mario tune
    marioMasterIdx = (uint8_t)(sizeof(MarioMaster) - 2);
    marioIdx = marioIdxEnd;
    marioPauze = GLCD_TRUE;
#else
    alarmTone = 0;
#endif
  }
}
#endif

//
// Time signal handler
//
// Read and sync the RTC with internal system time. It can result in a
// a Monochron time event, alarm trip event or alarm-end event when
// appropriate.
// Runs at about 30Hz, but will sync time considerably less due to time
// dividers.
//
#ifdef EMULIN
void stubTimer (void)
#else
SIGNAL (TIMER2_OVF_vect)
#endif
{
  wdt_reset();
#ifdef EMULIN
  if (t2divider1 == 0)
#else
#ifdef BACKLIGHT_ADJUST
  if (t2divider1 == TIMER2_RETURN_1)
#else
  if (t2divider1 == 5)
#endif
#endif
  {
    t2divider1 = 0;
  }
  else
  {
    t2divider1++;
    return;
  }

  // This occurs at approx 5.7Hz or 8.5Hz.
  // For this refer to defs of TIMER2_RETURN_x in ratt.h.
  uint8_t last_s = time_s;
  uint8_t last_m = time_m;
  uint8_t last_h = time_h;

  //DEBUG(putstring_nl("* RTC"));

  // Check the alarm switch state
  alarmStateSet();

  // Get RTC time and compare with saved one
  readi2ctime();
  if (time_h != last_h)
  {
    old_h = last_h;
    old_m = last_m;
  }
  else if (time_m != last_m)
  {
    old_m = last_m;
  }

  if (time_s != last_s)
  {
    // Do admin on countdown timers
    if (timeoutcounter)
      timeoutcounter--;
    if (alarming == GLCD_TRUE && snoozeTimer > 0)
    {
      if (snoozeTimer == 1)
      {
        // Init alarm data at starting positions right before we
        // return from snooze
#ifdef MARIO
        marioMasterIdx = (uint8_t)(sizeof(MarioMaster) - 2);
        marioIdx = marioIdxEnd;
        marioPauze = GLCD_TRUE;
#else
        alarmTone = 0;
#endif
        DEBUGP("Alarm -> Snooze timeout");
      }
      snoozeTimer--;
    }
    if (alarming == GLCD_TRUE && alarmTimer > 0)
    {
      alarmTimer--;
    }
    DEBUG(putstring("**** "));
    DEBUG(uart_putw_dec(time_h));
    DEBUG(uart_putchar(':'));
    DEBUG(uart_putw_dec(time_m));
    DEBUG(uart_putchar(':'));
    DEBUG(uart_putw_dec(time_s));
    DEBUG(putstring_nl(""));
  }

  // If we're in the setup menu we have a continuous time update
  // except when editing time itself or when we're changing
  // menu (screenmutex)
  if ((displaymode == SET_ALARM || displaymode == SET_DATE ||
       displaymode == SET_REGION || displaymode == SET_BRIGHTNESS ||
       displaymode == SET_DISPLAY) && !screenmutex)
  {
    glcdSetAddress(MENU_INDENT + 12 * 6, 2);
    glcdPrintNumberFg(time_h);
    glcdWriteCharFg(':');
    glcdPrintNumberFg(time_m);
    glcdWriteCharFg(':');
    glcdPrintNumberFg(time_s);
  }

  // Signal a clock time event only when the previous has not been
  // processed. This prevents a race condition on time data between
  // the timer handler and the functional clock handler. The functional
  // clock handler will clear the event after which a new time event
  // can be set.
  if (time_event == GLCD_FALSE && new_ts != time_s)
  {
    new_ts = time_s;
    new_tm = time_m;
    new_th = time_h;
    new_dd = date_d;
    new_dm = date_m;
    new_dy = date_y;
    DEBUG(putstring_nl("Raise time event"));
    time_event = GLCD_TRUE;
  }

  // Check if alarm has timed out (people sometimes do not wake up by alarm)
  if (alarmOn == GLCD_TRUE && alarming == GLCD_TRUE && alarmTimer == 0)
  {
    DEBUG(putstring_nl("Alarm -> Timeout"));
    alarming = GLCD_FALSE;
    snoozeTimer = 0;
    alarmTimer = -1;
  }
    
  // Check if we have an alarm set
  if (alarmOn == GLCD_TRUE && alarming == GLCD_FALSE &&
       time_s == 0 && time_m == mcAlarmM && time_h == mcAlarmH)
  {
    DEBUG(putstring_nl("Alarm -> Tripped"));
    alarming = GLCD_TRUE;
    alarmTimer = MAXALARM;
  }
  
  // Control timeout counters. Note this is tricky stuff since entering
  // this code section is also influenced by the t2divider1 counter at
  // the top of this function. With the current settings this code section
  // is entered about once per second.
  // To use this, uncomment the t2divider2 declaration at the top of this
  // file, the code section below and then add timeout counter logic.
  /*if (t2divider2 == TIMER2_RETURN_2)
  {
    t2divider2 = 0;
  }
  else
  {
    t2divider2++;
    return;
  }
  // Add your timeout counter functionality here
  */
}

#ifndef EMULIN
SIGNAL(TIMER1_OVF_vect) {
  PIEZO_PORT ^= _BV(PIEZO);
}
#endif

#ifndef EMULIN
SIGNAL(TIMER1_COMPA_vect)
{
  PIEZO_PORT ^= _BV(PIEZO);
}
#endif

//
// Function: alarmStateSet
//
// Turn on/off the alarm based on the alarm switch position
//
void alarmStateSet(void)
{
  if (ALARM_PIN & _BV(ALARM))
  {
    // Turn off alarm if needed
    if (alarmOn == GLCD_TRUE)
    {
      DEBUGP("Alarm -> Inactive");
      alarmOn = GLCD_FALSE;
      snoozeTimer = 0;
      alarmTimer = 0;
      if (alarming == GLCD_TRUE)
      {
        // If there is audible alarm turn it off
        DEBUGP("Alarm -> Off");
        alarming = GLCD_FALSE;
        TCCR1B = 0;
#ifdef MARIO
        // On next audible alarm start at beginning of Mario tune
        marioMasterIdx = (uint8_t)(sizeof(MarioMaster) - 2);
        marioIdx = marioIdxEnd;
        marioPauze = GLCD_TRUE;
#else
        alarmTone = 0;
#endif
        // Turn off piezo
        PIEZO_PORT &= ~_BV(PIEZO);
      } 
    }
  }
  else
  {
    // Turn on functional alarm if needed
    if (alarmOn == GLCD_FALSE)
    {
      DEBUGP("Alarm -> Active");
      // Alarm on!
      alarmOn = GLCD_TRUE;
      // Reset snoozing and alarm
      snoozeTimer = 0;
      alarmTimer = 0;
#ifdef MARIO
      // On next audible alarm start at beginning of Mario tune
      marioMasterIdx = (uint8_t)(sizeof(MarioMaster) - 2);
      marioIdx = marioIdxEnd;
      marioPauze = GLCD_TRUE;
#else
      alarmTone = 0;
#endif
    }   
  }
}

//
// Function: alarmTimeGet
//
// Get the requested alarm time from the eeprom
//
void alarmTimeGet(uint8_t alarmId, volatile uint8_t *hour, volatile uint8_t *min)
{
  uint8_t *aHour, *aMin;

  if (alarmId == 0)
  {
    aHour = (uint8_t *)EE_ALARM_HOUR;
    aMin = (uint8_t *)EE_ALARM_MIN;
  }
  else if (alarmId == 1)
  {
    aHour = (uint8_t *)EE_ALARM_HOUR2;
    aMin = (uint8_t *)EE_ALARM_MIN2;
  }
  else if (alarmId == 2)
  {
    aHour = (uint8_t *)EE_ALARM_HOUR3;
    aMin = (uint8_t *)EE_ALARM_MIN3;
  }
  else
  {
    aHour = (uint8_t *)EE_ALARM_HOUR4;
    aMin = (uint8_t *)EE_ALARM_MIN4;
  }

  *hour = eeprom_read_byte(aHour) % 24;
  *min = eeprom_read_byte(aMin) % 60;
}

//
// Function: alarmTimeSet
//
// Save the requested alarm time in the eeprom
//
void alarmTimeSet(uint8_t alarmId, volatile uint8_t hour, volatile uint8_t min)
{
  uint8_t *aHour, *aMin;

  if (alarmId == 0)
  {
    aHour = (uint8_t *)EE_ALARM_HOUR;
    aMin = (uint8_t *)EE_ALARM_MIN;
  }
  else if (alarmId == 1)
  {
    aHour = (uint8_t *)EE_ALARM_HOUR2;
    aMin = (uint8_t *)EE_ALARM_MIN2;
  }
  else if (alarmId == 2)
  {
    aHour = (uint8_t *)EE_ALARM_HOUR3;
    aMin = (uint8_t *)EE_ALARM_MIN3;
  }
  else
  {
    aHour = (uint8_t *)EE_ALARM_HOUR4;
    aMin = (uint8_t *)EE_ALARM_MIN4;
  }

  eeprom_write_byte(aHour, hour);    
  eeprom_write_byte(aMin, min);    
}

//
// Function: beep
//
// Sound beep.
// Note: The beep duration granularity is 25 msec
//
void beep(uint16_t freq, uint8_t duration)
{
#ifdef EMULIN
  stubBeep(freq, duration);
#else
  uint8_t i = duration;

  // Start the beep 
  TCCR1A = 0; 
  TCCR1B = _BV(WGM12) | _BV(CS10); // CTC with fastest timer
  TIMSK1 = _BV(TOIE1) | _BV(OCIE1A);
  OCR1A = (F_CPU / freq) / 2;

  // Wait time 
  while (i >= 25)
  {
    _delay_ms(25);
    i = i - 25;
  }

  // Turn off piezo
  TCCR1B = 0;
  PIEZO_PORT &= ~_BV(PIEZO);
#endif
}

//
// Function: dotw
//
// Return the day number of the week
//
uint8_t dotw(uint8_t mon, uint8_t day, uint8_t yr)
{
  uint16_t month, year;

  // Calculate day of the week
  month = mon;
  year = 2000 + yr;
  if (mon < 3)
  {
    month += 12;
    year -= 1;
  }
  return (day + (2 * month) + (6 * (month + 1) / 10) + year +
    (year / 4) - (year / 100) + (year / 400) + 1) % 7;
}

//
// Function: i2bcd
//
// Convert value to BCD
//
inline uint8_t i2bcd(uint8_t x)
{
  return ((x / 10) << 4) | (x % 10);
}

//
// Function: init_eeprom
//
// Initialize the eeprom. This should occur only in rare occasions as
// once it is set it should stay initialized forever.
//
void init_eeprom(void)
{
  // Set eeprom to a default state.
  if (eeprom_read_byte((uint8_t *)EE_INIT) != EE_INITIALIZED)
  {
    eeprom_write_byte((uint8_t *)EE_ALARM_HOUR, 8);
    eeprom_write_byte((uint8_t *)EE_ALARM_MIN, 0);
    eeprom_write_byte((uint8_t *)EE_BRIGHT, OCR2A_VALUE);
    eeprom_write_byte((uint8_t *)EE_VOLUME, 1);
    eeprom_write_byte((uint8_t *)EE_REGION, DATELONG_DOW);
    eeprom_write_byte((uint8_t *)EE_TIME_FORMAT, TIME_24H);
    eeprom_write_byte((uint8_t *)EE_BGCOLOR, 0);
    eeprom_write_byte((uint8_t *)EE_ALARM_HOUR2, 9);
    eeprom_write_byte((uint8_t *)EE_ALARM_MIN2, 15);
    eeprom_write_byte((uint8_t *)EE_ALARM_HOUR3, 10);
    eeprom_write_byte((uint8_t *)EE_ALARM_MIN3, 30);
    eeprom_write_byte((uint8_t *)EE_ALARM_HOUR4, 11);
    eeprom_write_byte((uint8_t *)EE_ALARM_MIN4, 45);
    eeprom_write_byte((uint8_t *)EE_ALARM_SELECT, 0);
    eeprom_write_byte((uint8_t *)EE_INIT, EE_INITIALIZED);
  }
}

//
// Function: leapyear
//
// Identify whether a year is a leap year
//
uint8_t leapyear(uint16_t y)
{
  return ( (!(y % 4) && (y % 100)) || !(y % 400));
}

//
// Function: mchronTimeInit
//
// Re-initialize the functional Monochron clock time. It will discard a
// pending time event (may be zero seconds old, a few seconds or even
// minutes) and will create a fresh time event that is based on *now*.
//
void mchronTimeInit(void)
{
  mcClockTimeEvent = GLCD_FALSE;
#ifndef EMULIN
  // First wait for a stable state (=registered time event)
  while (time_event == GLCD_FALSE);
  // Then force a re-init of the monochron time upon scan restart
  new_ts = 60;
  // Then restart the time scan mechanism
  DEBUGP("Clear time event");
  time_event = GLCD_FALSE;
  // And finally wait again for a stable situation (< 175 msec)
  while (time_event == GLCD_FALSE);
#else
  // As the emulator event loop is not in a separate thread nor is
  // interrupt driven we have to do things a bit differently
  DEBUGP("Clear time event");
  time_event = GLCD_FALSE;
  new_ts = 60;
  while (time_event == GLCD_FALSE)
    stubTimer();
#endif
  mcClockTimeEvent = GLCD_TRUE;
}

//
// Function: readi2ctime
//
// Read the real-time clock (RTC)
//
uint8_t readi2ctime(void)
{
  uint8_t regaddr = 0, r;
  uint8_t clockdata[8];
  
  // Check the time from the RTC
  cli();
  r = i2cMasterSendNI(0xD0, 1, &regaddr);

  if (r != 0)
  {
    DEBUG(putstring("Reading i2c data: "));
    DEBUG(uart_putw_dec(r));
    DEBUG(putstring_nl(""));
    while(1)
    {
      sei();
      beep(4000, 100);
      _delay_ms(100);
      beep(4000, 100);
      _delay_ms(1000);
    }
  }

  r = i2cMasterReceiveNI(0xD0, 7, &clockdata[0]);
  sei();

  if (r != 0)
  {
    DEBUG(putstring("Reading i2c data: "));
    DEBUG(uart_putw_dec(r));
    DEBUG(putstring_nl(""));
    while(1)
    {
      beep(4000, 100);
      _delay_ms(100);
      beep(4000, 100);
      _delay_ms(1000);
    }
  }

  time_s = ((clockdata[0] >> 4) & 0x7) * 10 + (clockdata[0] & 0xF);
  time_m = ((clockdata[1] >> 4) & 0x7) * 10 + (clockdata[1] & 0xF);
  if (clockdata[2] & _BV(6))
  {
    // "12 hr" mode
    time_h = ((clockdata[2] >> 5) & 0x1) * 12 + 
      ((clockdata[2] >> 4) & 0x1) * 10 + (clockdata[2] & 0xF);
  }
  else
  {
    time_h = ((clockdata[2] >> 4) & 0x3) * 10 + (clockdata[2] & 0xF);
  }
  
  date_d = ((clockdata[4] >> 4) & 0x3) * 10 + (clockdata[4] & 0xF);
  date_m = ((clockdata[5] >> 4) & 0x1) * 10 + (clockdata[5] & 0xF);
  date_y = ((clockdata[6] >> 4) & 0xF) * 10 + (clockdata[6] & 0xF);

  return clockdata[0] & 0x80;
}

//
// Function: rtcTimeInit
//
// Initialize RTC time data for first time use
//
void rtcTimeInit(void)
{
  // Talk to clock
  i2cInit();

  if (readi2ctime())
  {
    DEBUGP("Uh oh, RTC was off, lets reset it!");
    writei2ctime(00, 00, 12, 0, 1, 1, 15); // Noon Jan 1 2015
  }

  readi2ctime();

  DEBUG(putstring("\n\rread "));
  DEBUG(uart_putw_dec(time_h));
  DEBUG(uart_putchar(':'));
  DEBUG(uart_putw_dec(time_m));
  DEBUG(uart_putchar(':'));
  DEBUG(uart_putw_dec(time_s));

  DEBUG(uart_putchar('\t'));
  DEBUG(uart_putw_dec(date_d));
  DEBUG(uart_putchar('/'));
  DEBUG(uart_putw_dec(date_m));
  DEBUG(uart_putchar('/'));
  DEBUG(uart_putw_dec(date_y));
  DEBUG(putstring_nl(""));

  TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20); // div by 1024
  // Overflow ~30Hz = 8MHz/(255 * 1024)

  // Enable interrupt
  TIMSK2 = _BV(TOIE2);

  sei();
}

//
// Function: snoozeSet
//
// Make the alarm go snoozing
//
void snoozeSet(void)
{
  DEBUGP("Alarm -> Snooze");
  snoozeTimer = MAXSNOOZE;
  alarmTimer = MAXALARM + MAXSNOOZE;
  TCCR1B = 0;
  // Turn off piezo
  PIEZO_PORT &= ~_BV(PIEZO);
  // Force a clock to display the time
  displaymode = SHOW_TIME;
}

//
// Function: writei2ctime
//
// Set the real-time clock (RTC)
//
void writei2ctime(uint8_t sec, uint8_t min, uint8_t hr, uint8_t day,
		  uint8_t date, uint8_t mon, uint8_t yr)
{
  uint8_t clockdata[8] = {0,0,0,0,0,0,0,0};

  clockdata[0] = 0;           // address
  clockdata[1] = i2bcd(sec);  // s
  clockdata[2] = i2bcd(min);  // m
  clockdata[3] = i2bcd(hr);   // h
  clockdata[4] = i2bcd(day);  // day
  clockdata[5] = i2bcd(date); // date
  clockdata[6] = i2bcd(mon);  // month
  clockdata[7] = i2bcd(yr);   // year
  
  cli();
  uint8_t r = i2cMasterSendNI(0xD0, 8, &clockdata[0]);
  sei();

  // Not able to set the RTC. Beep forever to indicate we're screwed.
  if (r != 0)
  {
    while(1)
    {
      beep(4000, 100);
      _delay_ms(100);
      beep(4000, 100);
      _delay_ms(1000);
    }
  }
}

