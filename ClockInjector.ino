#include <ClickButton.h>
#include <Event.h>
#include <Timer.h>

#include <MIDI.h>
#include <midi_Defs.h>
#include <midi_Message.h>
#include <midi_Namespace.h>
#include <midi_Settings.h>

/*

  AUTHOR is Thomas Bucsics

  This little tool was made for Arduino and is to be used with
  a SparkFun Midi Shield.

  It is made for Markus Schrodt as a workaround for his Atari's
  delayed processing of MIDI messages by injecting additional
  clock messages after a start message.

  It behaves as follows:
  Only a single channel is manipulated - select it by changing the CHANNEL 
  define in the source code.

  Midi Thru is implemented in the code, so all incoming messages are forwarded,
  EXCEPT if a clock ignore triggered manually (see below).

  MANUAL INJECT: 
  Button D2 manually injects a clock message

  MANUAL IGNORE: 
  Button D3 manually ignores the next clock message

  AUTO-INJECT: 
  After a start message is received, m clock messages are
  injected, each n milliseconds after an incoming clock message.

  Like this:
  Start -> Clock -> Inject after n ms -> Clock -> Inject after n ms (repeat m times)

  So only clock is inserted per clock received, up to a maximum of m.

  m (the number of clock messages to inject) is regulated by potentiometer A0.
  Minimum is zero, maximum is 15 clocks to inject.

  n (the delay after an external clock message) is regulated by potentiometer A1.
  Minimum is Zero, maximum is 51ms

  A Start message will reset the injection counter (so m messages will be injected).
  Stop and Continue messages are ignored.
*/

#define KNOB1 0
#define KNOB2 1
#define STAT1 7
#define STAT2 6
#define CHANNEL 1
#define BUTTON1 2
#define BUTTON2 3
#define LED_PULSE_DURATION 1

ClickButton button1(BUTTON1);
ClickButton button2(BUTTON2);

int clocksToInject;
int clockDelay;
int clocksLeftToInject;

Timer pulseTimer;
Timer injectorTimer;
int8_t injectorId = -1;

boolean ignoreClockScheduled = false;

MIDI_CREATE_DEFAULT_INSTANCE();

// Initialize everything
void setup()
{
  Serial.begin(9600);

  pinMode(STAT1, OUTPUT);
  pinMode(STAT2, OUTPUT);
  digitalWrite(STAT2, HIGH);

  pinMode(BUTTON1, INPUT);
  pinMode(BUTTON2, INPUT);

  digitalWrite(BUTTON1, HIGH);
  digitalWrite(BUTTON2, HIGH);

  button1.multiclickTime = 1;

  clocksLeftToInject = 0;

  MIDI.begin(0);
  MIDI.turnThruOff();
  MIDI.setHandleClock(HandleClock);
  MIDI.setHandleStart(HandleStart);

  Serial.println("Initialization complete");
}

// Main loop
void loop()
{
  processInputs();
  if (MIDI.read())
  {
    if (MIDI.getChannel() == CHANNEL)  // Is there a MIDI message incoming on our channel?
    {
      switch (MIDI.getType())     // Get the type of the message we caught
      {
        case midi::Clock :
          HandleClock();
          break;
        case midi::Start :
          HandleStart();
          // no break here, since we actually want to forward it
        default:
          // Forward anything else
          forwardMostRecentMidiMessage();
          break;
      }
    } else
    {
      // Forward anything else
      forwardMostRecentMidiMessage();
    }

  }
  updateTimers();
}

void forwardMostRecentMidiMessage()
{
  MIDI.send(MIDI.getType(), MIDI.getData1(), MIDI.getData2(), MIDI.getChannel());
}

void updateTimers()
{
  injectorTimer.update();
  pulseTimer.update();
}

void processInputs()
{
  processKnobs();
  processButtons();
}

void processButtons()
{
  button1.Update();
  if (button1.clicks != 0)
  {
    injectClock();
  }

  button2.Update();
  if (button2.clicks != 0)
  {
    Serial.println("Ignoring next clock");
    ignoreClockScheduled = true;
  }
}

void processKnobs()
{
  // We want 16 steps for the clock injection count
  clocksToInject = analogRead(KNOB1) / 64;
  setLevelLed(clocksToInject, STAT1);

  // Max is 51ms (24 clocks per Beat - this should be plenty!)
  clockDelay = analogRead(KNOB2) / 20;
}


void injectClock()
{
  Serial.println("Injecting Clock");
  MIDI.send(midi::Clock, 0, 0, CHANNEL);
  pulseTimer.pulseImmediate(STAT2, LED_PULSE_DURATION, LOW);
}


// Set the level LED so the user can see if they are changing anything
// It basically turns on or off alternatingly once you reach a value threshhold
void setLevelLed(int level, int ledNo)
{
  if (level % 2 == 0) {
    digitalWrite(ledNo, HIGH);
  }
  else {
    digitalWrite(ledNo, LOW);
  }
}

void HandleClock(void)
{  
  if (clocksLeftToInject > 0)
  {
    injectorId = injectorTimer.after(clockDelay, injectClock);
    Serial.println(clockDelay);
    clocksLeftToInject--;
  }

  if (ignoreClockScheduled)
  {
    ignoreClockScheduled = false;
    Serial.println("Clock ignored");
    return;
  }

  MIDI.send(midi::Clock, 0, 0, CHANNEL);
}

void HandleStart(void)
{
  Serial.println("Start received");
  clocksLeftToInject = clocksToInject;
  Serial.print("Injecting clocks: ");
  Serial.println(clocksLeftToInject);
}
