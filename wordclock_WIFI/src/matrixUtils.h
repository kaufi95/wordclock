#ifndef MATRIXUTILS_H
#define MATRIXUTILS_H

#include <Arduino.h>
#include <FastLED.h>

bool showEsIst(uint8_t minutes);
void turnLedsOn(uint16_t start, uint16_t end, CRGB* leds, uint8_t red, uint8_t green, uint8_t blue);

#endif