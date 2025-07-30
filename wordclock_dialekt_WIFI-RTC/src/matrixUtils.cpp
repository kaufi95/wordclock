#include <Arduino.h>
#include <FastLED.h>
#include "matrixUtils.h"

// determine if "es isch" is shown
bool showEsIst(uint8_t minutes)
{
    bool randomized = random() % 2 == 0;
    return randomized || minutes % 30 < 5;
}

// turn on LEDs directly using LED indices
void turnLedsOn(uint16_t start, uint16_t end, CRGB* leds, uint8_t red, uint8_t green, uint8_t blue)
{
    for (uint16_t i = start; i <= end; i++)
    {
        leds[i] = CRGB(red, green, blue);
    }
}