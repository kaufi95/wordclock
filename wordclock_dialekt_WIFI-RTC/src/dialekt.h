#ifndef DIALEKT_H
#define DIALEKT_H

#include <Arduino.h>
#include <TimeLib.h>
#include <FastLED.h>
#include "matrixUtils.h"

namespace dialekt
{
    void timeToLeds(time_t time, CRGB* leds, uint8_t red, uint8_t green, uint8_t blue);
}

#endif