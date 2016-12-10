//*****************************************************************************
// Filename : 'buttons.c'
// Title    : Button debouncing/switches handling
//*****************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "util.h"
#include "monomain.h"
#include "ks0108.h"
#include "buttons.h"

//
// Monochron has a built-in Analog to Digital Converter (ADC).
// For more info on what it does and how it works refer to:
// http://apcmag.com/arduino-analog-to-digital-converter-how-it-works.htm/
//
// Normally an ADC is used for measuring signals, and therefor requires a fast
// and continuous ADC scan, called a conversion. The ADC can operate between a
// 615 KHz and 9.6 KHz conversion rate. Knowing that processing a conversion
// result requires cpu resources, it means that having a high conversion rate
// has a negative impact on the speed of functional clock and graphics code as
// its execution is interrupted more often.
//
// The original Monochron firmware is configured to use a continuous 19.2 kHz
// conversion rate. For scanning buttons pressed by humans however this is
// overkill. Instead, we'll be using the 9.6 kHz rate (leading to more accurate
// ADC samples) but combine that with an ADC scan schedule using a countdown
// timer in the Monochron 1-msec interrupt handler. This will greatly reduce
// the actual button ADC sample rate. An initial performance test run after
// implementing this scheme shows an average performance increase for glcd
// graphics at around 11% (Emuchron v3.0, avr-gcc 4.8.1), while keeping the
// same button UI experience. It is assumed that functional clock code will
// show improved execution performance with identical numbers. 
//

// The following defines are used to switch between a fast and slow ADC
// conversion rate for the buttons. When a button is pressed and/or hold, the
// next conversion is done fast to make sure that no button event is missed.
// When all buttons are released, the ADC conversion scan mechanism switches
// to a slow conversion rate, thus freeing up cpu resources for Monochron
// functional clock and glcd graphics purposes.
// WARNING: *Never* set BTN_TICK_CONV_FAST/SLOW to 1 as it may create a race
// condition between the 1-msec timer and the button conversion handler.
#define BTN_TICK_CONV_FAST_MS	2	// 2 msec = max 500 Hz
#define BTN_TICK_CONV_SLOW_MS	20	// 20 msec = max 50 Hz

// This variable holds a button being pressed. It is used to detect a single
// button press only, so NOT a press-hold event. It will hold only one button.
// WARNING: When used in functional code, that code *must* clear its value to
// BTN_NONE. Clear the value as soon as possible or else a new button press may
// get lost as the previous press has not been cleared fast enough.
volatile uint8_t btnPressed = BTN_NONE;

// This variable holds the button when press-hold longer than 250 msec.
// NOTE: When a press-hold is detected, the btnPressed value will be cleared.
// NOTE: Currently only the '+' button is supported for press-hold. When the
// button is released, this module will clear its value to BTN_NONE.
volatile uint8_t btnHold = BTN_NONE;

// Countdown timer in msec to start another conversion, thus driving the ADC
// conversion rate.
// It is decremented in the monomain.c [firmware] 1 msec handler.
volatile uint8_t btnTickerConv = 0;

// Countdown timer in msec to detect '+' button press-hold.
// It is decremented in the monomain.c [firmware] 1 msec handler.
volatile uint8_t btnTickerHold = 0;

// The button hold release request and confirm variables allow for
// proper 'high speed incrementing' behavior when setting config values
volatile uint8_t btnHoldRelReq = GLCD_FALSE;
volatile uint8_t btnHoldRelCfm = GLCD_FALSE;

// Local variable to detect changes in buttons being pressed
static volatile uint8_t btnLastState = BTN_NONE;

// Local function prototypes
static uint16_t btnADCRead(void);

//
// Function: btnADCRead
//
// Do an ADC (Analog to Digital Converter) conversion and return its result.
//
static uint16_t btnADCRead(void)
{
  // No interrupt
  ADCSRA &= ~_BV(ADIE);
  // Start a conversion
  ADCSRA |= _BV(ADSC);
  while (!(ADCSRA & _BV(ADIF)));
  return ADC;
}

//
// Function: btnConvStart
//
// Start a button ADC conversion. When complete the ADC_vect handler is called.
//
void btnConvStart(void)
{
  ADCSRA |= _BV(ADIE) | _BV(ADSC);
}

//
// Function: btnInit
//
// Initializes the Monochron buttons and alarm switch hardware and runs the
// first button ADC conversion.
//
void btnInit(void)
{
  // Alarm pin requires a pullup
  ALARM_DDR &= ~_BV(ALARM);
  ALARM_PORT |= _BV(ALARM);

  // Alarm switching is detected by using the pin change interrupt
  PCICR = _BV(PCIE0);
  PCMSK0 |= _BV(ALARM);

  // The buttons are totem pole'd together so we can read the buttons with
  // one pin set up ADC
  ADMUX = 2;      // Listen to ADC2 for button presses
  ADCSRB = 0;     // Free running mode

  // Enable ADC and interrupts and prescale down to requested sample rate
  // using ADPS0/1/2.
  // Sample rate 19.2 KHz
  //ADCSRA = _BV(ADEN) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1);
  // Sample rate 9.6 KHz
  ADCSRA = _BV(ADEN) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);

  // Start first ADC button conversion
  ADCSRA |= _BV(ADSC);
}

//
// Every time the ADC finishes a conversion, we'll see whether the buttons
// have changed. The end result is stored in btnPressed and btnHold.
// When the conversion result is processed, schedule the next conversion
// using countdown ticker btnTickerConv.
//
SIGNAL(ADC_vect)
{
  uint16_t reading, reading2;
  sei();

  // We get called when ADC is ready so no need to request a conversion
  reading = ADC;

  if (reading > 735)
  {
    // No presses
    btnHold = BTN_NONE;
    btnLastState = BTN_NONE;
    if (btnHoldRelReq == GLCD_TRUE)
    {
      btnHoldRelCfm = GLCD_TRUE;
      btnHoldRelReq = GLCD_FALSE;
      DEBUGP("rlc");
    }
    btnTickerConv = BTN_TICK_CONV_SLOW_MS;
    return;
  }
  else if (reading > 610)
  {
    // Button 3 "+" pressed
    if (!(btnLastState & BTN_PLUS))
    {
      // Was not pressed before, debounce by taking another reading
      _delay_ms(10);
      reading2 = btnADCRead();
      if (reading2 > 735 || reading2 < 610)
      {
        // Was a bounce, ignore it
        btnTickerConv = BTN_TICK_CONV_FAST_MS;
        return;
      }

      // See if we're press-and-holding.
      // The hold counter is decremented by the 1-msec timer.
      btnTickerHold = 250;
      while (btnTickerHold)
      {
        reading2 = btnADCRead();
        if (reading2 > 735 || reading2 < 610)
        {
          // Button press-hold was released; signal a single-press
          btnLastState = BTN_NONE;
          DEBUGP("b3");
          btnPressed = BTN_PLUS;
          btnTickerConv = BTN_TICK_CONV_FAST_MS;
          return;
        }
      }
      // 0.25 second later we have press-hold
      btnPressed = BTN_NONE;
      btnLastState = BTN_PLUS;
      btnHold = BTN_PLUS;
    }
  }
  else if (reading > 270)
  {
    // Button 2 "SET" pressed
    if (!(btnLastState & BTN_SET))
    {
      // Was not pressed before, debounce by taking another reading
      _delay_ms(10);
      reading2 = btnADCRead();
      if (reading2 > 610 || reading2 < 270)
      {
        // Was a bounce, ignore it
        btnTickerConv = BTN_TICK_CONV_FAST_MS;
        return;
      }
      DEBUGP("b2");
      btnPressed = BTN_SET;
    }
    btnLastState = BTN_SET;
    btnHold = BTN_NONE;
  }
  else
  {
    // Button 1 "MENU" pressed
    if (!(btnLastState & BTN_MENU))
    {
      // Was not pressed before, debounce by taking another reading
      _delay_ms(10);
      reading2 = btnADCRead();
      if (reading2 > 270)
      {
        // Was a bounce, ignore it
        btnTickerConv = BTN_TICK_CONV_FAST_MS;
        return;
      }
      DEBUGP("b1");
      btnPressed = BTN_MENU;
    }
    btnLastState = BTN_MENU;
    btnHold = BTN_NONE;
  }

  // Something happened so do a fast conversion cycle
  btnTickerConv = BTN_TICK_CONV_FAST_MS;
}

//
// We use the pin change interrupts to detect when alarm switch changes.
//
SIGNAL(PCINT0_vect)
{
  // Allow interrupts while we're doing this
  sei();
  // Well, it turns out that interrupt changes are unreliable. A physical
  // on/off switch change sometimes generates jittered off->on->off->on
  // events that are too fast for the event handler, resulting in an
  // out-of-sync software switch state. It will lead to an alarm not being
  // fired when set, or an alarm being fired while being switched off.
  // Not good!
  // The remedy is to integrate a pin state check in the timer event handler.
  // It will detect a pin state not in realtime, so there will be a short
  // time lag (max ~75 msec) between a switch change and its processing in
  // software, but the good news is that it will be *reliable*.

  //setalarmstate();
}
