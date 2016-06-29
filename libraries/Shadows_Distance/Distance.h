#ifndef Distance_h
#define Distance_h

#include <Arduino.h>

#include <Streaming.h>

// Moteuino R4 pins already in use:
//  D2, D10-13 - transceiver
//  D9         - LED
//  D8, D11-13 - flash

#define PIN_GND 3   // DC return
#define PIN_VCC 4   // Vcc in range 3-5.5VDC
#define PIN_RX  6   // hold high/open for ranging. hold low to stop ranging.
#define PIN_PW  7   // pulse width representation with scale factor of 140 uS per inch

class Distance {
  public:
    // initialize location
    void begin();

    // calibrate sensor;  should be done periodically to adjust for changing temperature and humidity
    void calibrate();

    // read distance
    word read();
};

extern Distance D;

#endif