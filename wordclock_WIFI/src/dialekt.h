#ifndef DIALEKT_H
#define DIALEKT_H

#include <Arduino.h>
#include <TimeLib.h>
#include <Adafruit_NeoPixel.h>
#include "matrixUtils.h"

namespace dialekt
{
    void timeToLeds(time_t time, Adafruit_NeoPixel* strip, uint8_t red, uint8_t green, uint8_t blue);
}

#endif