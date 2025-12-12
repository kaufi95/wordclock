#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "matrixUtils.h"

// determine if "es isch" / "es ist" is shown
bool showEsIst(uint8_t minutes, uint8_t prefixMode)
{
    // prefixMode: 0 = always, 1 = random, 2 = off
    if (prefixMode == 2) {
        return false;  // OFF - never show
    } else if (prefixMode == 0) {
        return true;   // ALWAYS - always show
    } else {
        // RANDOM - cache decision per minute to avoid flickering
        static uint8_t lastMinute = 255;
        static uint8_t lastPrefixMode = 255;
        static bool cachedDecision = true;

        // When switching to Random mode, always show ES IST initially
        if (lastPrefixMode != prefixMode) {
            lastPrefixMode = prefixMode;
            lastMinute = minutes;
            cachedDecision = true;  // Show on first display
            return true;
        }

        // Re-randomize only when minute changes
        if (lastMinute != minutes) {
            lastMinute = minutes;
            cachedDecision = random() % 2 == 0;
        }

        return cachedDecision;
    }
}

// turn on LEDs directly using LED indices
void turnLedsOn(uint16_t start, uint16_t end, Adafruit_NeoPixel* strip, uint8_t red, uint8_t green, uint8_t blue)
{
    for (uint16_t i = start; i <= end; i++)
    {
        strip->setPixelColor(i, strip->Color(red, green, blue));
    }
}