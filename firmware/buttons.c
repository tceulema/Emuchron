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
#include "ratt.h"

// These store the current button states for all 3 buttons. We can 
// then query whether the buttons are pressed and released or pressed.
// This allows for 'high speed incrementing' when setting config values.
volatile uint8_t last_buttonstate = 0, just_pressed = 0, pressed = 0;
volatile uint8_t buttonholdcounter = 0;

void buttonsInit(void)
{
  // Alarm pin requires a pullup
  ALARM_DDR &= ~_BV(ALARM);
  ALARM_PORT |= _BV(ALARM);

  // Alarm switching is detected by using the pin change interrupt
  PCICR =  _BV(PCIE0);
  PCMSK0 |= _BV(ALARM);

  // The buttons are totem pole'd together so we can read the buttons with
  // one pin set up ADC
  ADMUX = 2;      // Listen to ADC2 for button presses
  ADCSRB = 0;     // Free running mode
  // Enable ADC and interrupts, prescale down to <200KHz
  ADCSRA = _BV(ADEN) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1); 
  ADCSRA |= _BV(ADSC); // start a conversion
}

uint16_t readADC(void)
{
  // Basically just the busy-wait code to read the ADC and return the value

  // No interrupt
  ADCSRA &= ~_BV(ADIE);
  // Start a conversion
  ADCSRA |= _BV(ADSC);
  while (! (ADCSRA & _BV(ADIF)));
  return ADC;
}

// Every time the ADC finishes a conversion, we'll see whether the
// buttons have changed
SIGNAL(ADC_vect)
{
  uint16_t reading, reading2;
  sei();

  // We get called when ADC is ready so no need to request a conversion
  reading = ADC;

  if (reading > 735)
  {
    // No presses
    pressed = 0;
    last_buttonstate = 0;
    
    // Start next conversion
    ADCSRA |= _BV(ADIE) | _BV(ADSC);
    return;
  }
  else if (reading > 610)
  {
    // Button 3 "+" pressed
    if (!(last_buttonstate & BTTN_PLUS))
    {
      // Was not pressed before, debounce by taking another reading
      _delay_ms(10);
      reading2 = readADC();
      if ( (reading2 > 735) || (reading2 < 610))
      {
	// Was a bounce, ignore it
        // Start next conversion
	ADCSRA |= _BV(ADIE) | _BV(ADSC);
	return;
      }

      // See if we're press-and-holding.
      // The hold counter is decremented by the 1-msec timer.
      buttonholdcounter = 150;
      while (buttonholdcounter)
      {
	reading2 = readADC();
	if ( (reading2 > 735) || (reading2 < 610))
        {
	  // Button was released
	  last_buttonstate &= ~BTTN_PLUS;
  
	  DEBUG(putstring_nl("b3"));
	  just_pressed |= BTTN_PLUS;
          // Start next conversion
	  ADCSRA |= _BV(ADIE) | _BV(ADSC);
	  return;
	}
      }
      // 0.15 second later...
      last_buttonstate |= BTTN_PLUS;
      // The button was held down (fast advance)
      pressed |= BTTN_PLUS;
    }
  }
  else if (reading > 270)
  {
    // Button 2 "SET" pressed
    if (!(last_buttonstate & BTTN_SET))
    {
      // Was not pressed before, debounce by taking another reading
      _delay_ms(10);
      reading2 = readADC();
      if ( (reading2 > 610) || (reading2 < 270))
      {
	// Was a bounce, ignore it
        // Start next conversion
	ADCSRA |= _BV(ADIE) | _BV(ADSC);	
	return;
      }
      DEBUG(putstring_nl("b2"));
      just_pressed |= BTTN_SET;
    }
    last_buttonstate |= BTTN_SET;
    // The button was held down
    pressed |= BTTN_SET;
  }
  else
  {
    // Button 1 "MENU" pressed
    if (!(last_buttonstate & BTTN_MENU))
    {
      // Was not pressed before, debounce by taking another reading
      _delay_ms(10);
      reading2 = readADC();
      if (reading2 > 270)
      {
	// Was a bounce, ignore it
        // Start next conversion
	ADCSRA |= _BV(ADIE) | _BV(ADSC); 	
	return;
      }
      DEBUG(putstring_nl("b1"));
      just_pressed |= BTTN_MENU;
    }
    last_buttonstate |= BTTN_MENU;
    // The button was held down
    pressed |= BTTN_MENU;
  }

  // Start next conversion
  ADCSRA |= _BV(ADIE) | _BV(ADSC); 
}

// We use the pin change interrupts to detect when alarm switch changes
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
