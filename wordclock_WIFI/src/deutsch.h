#ifndef DEUTSCH_H
#define DEUTSCH_H

#include <Arduino.h>
#include <TimeLib.h>
#include <Adafruit_NeoPixel.h>
#include "matrixUtils.h"

namespace deutsch
{
    void timeToLeds(time_t time, Adafruit_NeoPixel* strip, uint8_t red, uint8_t green, uint8_t blue, uint8_t prefixMode, String* timeString);
}

#endif