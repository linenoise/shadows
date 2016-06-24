#include <Streaming.h>
#include <Metro.h>

#include <RFM69.h> // RFM69HW radio transmitter module
#include <SPI.h> // for radio board 
#include <SPIFlash.h>
#include <avr/wdt.h>
#include <WirelessHEX69.h>
#include <EEPROM.h>

#include <Network.h>

#include <FastLED.h>
#include <FiniteStateMachine.h>

#include "Animation.h"

// track our response using a finite state machine
// idle <--> outPlane <--> inPlane
void idleUpdate();
State idle = State(idleUpdate); // nothing going on
void inPlaneUpdate();
State inPlane = State(inPlaneUpdate); // sensors are picking up an object, and it's within the place of the triangle
FSM S = FSM(idle); // start idle

// track state
systemState lastState;

void setup() {

  Serial.begin(115200);

  // start the radio
  N.begin();

  // wait enough time to get a reprogram signal
  Metro startupDelay(1000UL);
  while (! startupDelay.check()) N.update();

  // startup animation
  A.begin();

}

void loop() {
  // update the radio traffic
  if ( N.update() ) N.printMessage();

  // check for system mode changes
  if ( N.getState() != lastState ) {
    if ( N.getState() == M_CALIBRATE ) {
      A.setAnimation(A_CALIBRATE, false);
    } else if ( N.getState() == M_PROGRAM ) {
      A.setAnimation(A_PROGRAM, false);
    } else if ( N.getState() == M_NORMAL ) {
      S.transitionTo( idle );
      A.setAnimation( A_IDLE, false );
    }
    lastState = N.getState();
  }
  
  // update the FSM, letting it set animations as needed.
  S.update();

  // update the animation
  A.update();
}

void idleUpdate() {
  // until we get normalized, stay idle
  if ( N.getState() != M_NORMAL ) return;

  // check for state changes
  static Metro goInPlaneTimeout(500UL);
  if ( !N.objectInPlane() ) goInPlaneTimeout.reset();

  // do we detect something out there?
  if ( goInPlaneTimeout.check() ) {
    Serial << F("State.  idle->outPlane.") << endl;
    S.transitionTo( inPlane );
    A.setAnimation( A_INPLANE, false );
  }
}

void inPlaneUpdate() {
  // until we get normalized, stay idle
  if ( N.getState() != M_NORMAL ) return;

  // drive shadow center according to intercept
  A.setCenter(
    map(
      constrain(N.myIntercept(), 0, SENSOR_DIST),  // constrain x to be [0, BASE_LEN]
      0, SENSOR_DIST, // map [0, SENSOR_DIST]
      0, NUM_LEDS - 1 // to [0, NUM_LEDS-1]
    )
  );

  // drive shadow extent according to range
  A.setExtent(
    map(
      constrain(N.myRange(), 0, HEIGHT_LEN),
      0, HEIGHT_LEN, // map [0, HEIGHT_LEN]
      0, NUM_LEDS / 2 // to [0, NUM_LEDS/2]
    )
  );

  // check for state changes
  static Metro goIdleTimeout(500UL);
  if ( N.objectInPlane() ) goIdleTimeout.reset();

  // nothing to see here or no longer in normal operation?
  if ( goIdleTimeout.check() || N.getState() != M_NORMAL ) {
    Serial << F("State.  inPlane->idle.") << endl;
    S.transitionTo( idle );
    A.setAnimation( A_IDLE, false );
  }

}

