/*

H E S C E I S C H O S
F Ü N F Z W A N Z I G
V I E R T E L Z E H N
F V O R L N O C H N S
H A L B H Z W O A N S
D R E I V S E C H S E
S I E B N E Z N Ü N E
F Ü N F E O A C H T E
V I E R E N Z E H N E
E L F E I Z W Ö L F E
       . . . .

*/

#include <Arduino.h>
#include <TimeLib.h>
#include <FastLED.h>
#include "matrixUtils.h"

namespace dialekt
{
    void hour_one();
    void hour_two();
    void hour_three();
    void hour_four();
    void hour_five();
    void min_five();
    void hour_six();
    void hour_seven();
    void hour_eight();
    void hour_nine();
    void hour_ten();
    void min_ten();
    void hour_eleven();
    void hour_twelve();
    void quarter();
    void twenty();
    void to();
    void after();
    void half();

    CRGB *leds;
    uint8_t red, green, blue;

    // converts time directly to LED array
    void timeToLeds(time_t time, CRGB* _leds, uint8_t _red, uint8_t _green, uint8_t _blue)
    {
        leds = _leds;
        red = _red;
        green = _green;
        blue = _blue;
        
        uint8_t hours = hour(time);
        uint8_t minutes = minute(time);

        // show "Es isch" randomized
        if (showEsIst(minutes))
        {
            Serial.print("Es isch ");
            turnLedsOn(1, 2, leds, red, green, blue);    // "ES" - LEDs 1-2
            turnLedsOn(5, 8, leds, red, green, blue);    // "ISCH" - LEDs 5-8
        }

        // show minutes
        if (minutes >= 5 && minutes < 10)
        {
            min_five();
            Serial.print(" ");
            after();
        }
        else if (minutes >= 10 && minutes < 15)
        {
            min_ten();
            Serial.print(" ");
            after();
        }
        else if (minutes >= 15 && minutes < 20)
        {
            quarter();
            Serial.print(" ");
            after();
        }
        else if (minutes >= 20 && minutes < 25)
        {
            twenty();
            Serial.print(" ");
            after();
        }
        else if (minutes >= 25 && minutes < 30)
        {
            min_five();
            Serial.print(" ");
            to();
            Serial.print(" ");
            half();
        }
        else if (minutes >= 30 && minutes < 35)
        {
            half();
        }
        else if (minutes >= 35 && minutes < 40)
        {
            min_five();
            Serial.print(" ");
            after();
            Serial.print(" ");
            half();
        }
        else if (minutes >= 40 && minutes < 45)
        {
            twenty();
            Serial.print(" ");
            to();
        }
        else if (minutes >= 45 && minutes < 50)
        {
            quarter();
            Serial.print(" ");
            to();
        }
        else if (minutes >= 50 && minutes < 55)
        {
            min_ten();
            Serial.print(" ");
            to();
        }
        else if (minutes >= 55 && minutes < 60)
        {
            min_five();
            Serial.print(" ");
            to();
        }

        Serial.print(" ");

        // convert hours to 12h format
        if (hours >= 12)
        {
            hours -= 12;
        }

        if (minutes >= 25)
        {
            hours++;
        }

        if (hours == 12)
        {
            hours = 0;
        }

        // show hours
        switch (hours)
        {
        case 0:
            // Zwölfe
            hour_twelve();
            break;
        case 1:
            // Oans
            hour_one();
            break;
        case 2:
            // Zwoa
            hour_two();
            break;
        case 3:
            // Drei
            hour_three();
            break;
        case 4:
            // Viere
            hour_four();
            break;
        case 5:
            // Fünfe
            hour_five();
            break;
        case 6:
            // Sechse
            hour_six();
            break;
        case 7:
            // Siebne
            hour_seven();
            break;
        case 8:
            // Achte
            hour_eight();
            break;
        case 9:
            // Nüne
            hour_nine();
            break;
        case 10:
            // Zehne
            hour_ten();
            break;
        case 11:
            // Elfe
            hour_eleven();
            break;
        }

        // Show minute dots (LEDs 110-113)
        for (uint8_t i = 0; i < (minutes % 5); i++) {
            leds[110 + i] = CRGB(red, green, blue);
        }

        Serial.print(" + ");
        Serial.print(minutes % 5);
        Serial.print(" min");
        Serial.println();
    }

    // ------------------------------------------------------------
    // numbers as labels

    void hour_one()
    {
        // OANS - Row 4 positions 7-10: LEDs 47-50
        Serial.print("oans");
        turnLedsOn(47, 50, leds, red, green, blue);
    }

    void hour_two()
    {
        // zwoa/zwei
        Serial.print("zwoa");
        turnLedsOn(49, 52, leds, red, green, blue);  // ZWOA
    }

    void hour_three()
    {
        // drei
        Serial.print("drei");
        turnLedsOn(62, 65, leds, red, green, blue);  // DREI
    }

    void hour_four()
    {
        // vier/e/vier
        Serial.print("viere");
        turnLedsOn(88, 92, leds, red, green, blue);  // VIERE
    }

    void hour_five()
    {
        // fünfe/fünf
        Serial.print("fünfe");
        turnLedsOn(83, 87, leds, red, green, blue);  // FÜNFE
    }

    void min_five()
    {
        // FÜNF - Row 1 positions 0-3: LEDs 21-18
        Serial.print("fünf");
        turnLedsOn(18, 21, leds, red, green, blue);
    }

    void hour_six()
    {
        // sechse/sechs
        Serial.print("sechse");
        turnLedsOn(55, 60, leds, red, green, blue);  // SECHSE
    }

    void hour_seven()
    {
        // siebne/sieben
        Serial.print("siebne");
        turnLedsOn(66, 71, leds, red, green, blue);  // "SIEBNE" - row 6, cols 0-5
    }

    void hour_eight()
    {
        // achte/acht
        Serial.print("achte");
        turnLedsOn(76, 80, leds, red, green, blue);  // "ACHTE" - row 7, cols 6-10
    }

    void hour_nine()
    {
        // nüne/neun
        Serial.print("nüne");
        turnLedsOn(73, 76, leds, red, green, blue);  // "NÜNE" - row 6, cols 7-10
    }

    void hour_ten()
    {
        // zehne/zehn
        Serial.print("zehne");
        turnLedsOn(89, 93, leds, red, green, blue);  // "ZEHNE" - row 8, cols 6-10
    }

    void min_ten()
    {
        // zehn/zehn
        Serial.print("zehn");
        turnLedsOn(29, 32, leds, red, green, blue);  // "ZEHN" - row 2, cols 7-10
    }

    void hour_eleven()
    {
        // elfe/elf
        Serial.print("elfe");
        turnLedsOn(99, 102, leds, red, green, blue);  // "ELFE" - row 9, cols 0-3
    }

    void hour_twelve()
    {
        // zwölfe/zwölf
        Serial.print("zwölfe");
        turnLedsOn(104, 109, leds, red, green, blue);  // "ZWÖLFE" - row 9, cols 5-10
    }

    void quarter()
    {
        // viertel
        Serial.print("viertel");
        turnLedsOn(22, 28, leds, red, green, blue);  // "VIERTEL" - row 2, cols 0-6
    }

    void twenty()
    {
        // zwanzig
        Serial.print("zwanzig");
        turnLedsOn(15, 21, leds, red, green, blue);  // "ZWANZIG" - row 1, cols 4-10
    }

    // ------------------------------------------------------------

    void to()
    {
        // VOR - Row 3 positions 1-3: LEDs 42-40
        Serial.print("vor");
        turnLedsOn(40, 42, leds, red, green, blue);
    }

    void after()
    {
        // NOCH - Row 3 positions 5-8: LEDs 38-35
        Serial.print("noch");
        turnLedsOn(35, 38, leds, red, green, blue);
    }

    void half()
    {
        // HALB - Row 4 positions 0-3: LEDs 44-47
        Serial.print("halb");
        turnLedsOn(44, 47, leds, red, green, blue);
    }

} // namespace dialekt