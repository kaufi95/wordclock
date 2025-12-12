#ifndef MATRIXUTILS_H
#define MATRIXUTILS_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

bool showEsIst(uint8_t minutes, uint8_t prefixMode);
void turnLedsOn(uint16_t start, uint16_t end, Adafruit_NeoPixel* strip, uint8_t red, uint8_t green, uint8_t blue);

#endif