/*

H E S C E I S T H L S
F Ü N F Z W A N Z I G
V I E R T E L Z E H N
F V O R L N A C H N S
H A L B H Z W E I N S
D R E I V S E C H S E
S I E B E N Z N E U N
F Ü N F E N A C H T E
V I E R N Z W Ö L F E
E L F Z E H N E U H R
       . . . .

*/

/*

  0   1   2   3   4   5   6   7   8   9  10
 21  20  19  18  17  16  15  14  13  12  11
 22  23  24  25  26  27  28  29  30  31  32
 43  42  41  40  39  38  37  36  35  34  33
 44  45  46  47  48  49  50  51  52  53  54
 65  64  63  62  61  60  59  58  57  56  55
 66  67  68  69  70  71  72  73  74  75  76
 87  86  85  84  83  82  81  80  79  78  77
 88  89  90  91  92  93  94  95  96  97  98
109 108 107 106 105 104 103 102 101 100  99
              110 111 112 113

*/

#include <Arduino.h>
#include <TimeLib.h>
#include <Adafruit_NeoPixel.h>
#include "matrixUtils.h"

namespace deutsch
{
    void hour_one(bool s);
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
    void uhr();

    Adafruit_NeoPixel *strip;
    uint8_t red, green, blue;

    // converts time directly to LED array
    void timeToLeds(time_t time, Adafruit_NeoPixel *_strip, uint8_t _red, uint8_t _green, uint8_t _blue)
    {
        strip = _strip;
        red = _red;
        green = _green;
        blue = _blue;

        uint8_t hours = hour(time);
        uint8_t minutes = minute(time);

        // show "Es ist"
        if (showEsIst(minutes))
        {
            // Es ist
            Serial.print("Es ist ");
            turnLedsOn(1, 2, strip, red, green, blue); // "ES" - LEDs 1-2
            turnLedsOn(5, 7, strip, red, green, blue); // "IST" - LEDs 5-7
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
            if (minutes >= 0 && minutes < 5)
            {
                hour_one(false);
            }
            else
            {
                hour_one(true);
            }
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

        if (minutes >= 0 && minutes < 5)
        {
            uhr();
        }

        // Show minute dots (LEDs 110-113)
        for (uint8_t i = 0; i < (minutes % 5); i++)
        {
            strip->setPixelColor(110 + i, strip->Color(red, green, blue));
        }

        Serial.print(" + ");
        Serial.print(minutes % 5);
        Serial.print(" min");
        Serial.println();
    }

    // ------------------------------------------------------------
    // numbers as labels

    void hour_one(bool s)
    {
        if (s)
        {
            Serial.print("eins");
            turnLedsOn(51, 54, strip, red, green, blue);
        }
        else
        {
            Serial.print("ein");
            turnLedsOn(51, 53, strip, red, green, blue);
        }
    }

    void hour_two()
    {
        Serial.print("zwei");
        turnLedsOn(49, 52, strip, red, green, blue);
    }

    void hour_three()
    {
        Serial.print("drei");
        turnLedsOn(62, 65, strip, red, green, blue);
    }

    void hour_four()
    {
        Serial.print("vier");
        turnLedsOn(88, 91, strip, red, green, blue);
    }

    void hour_five()
    {
        Serial.print("fünf");
        turnLedsOn(84, 87, strip, red, green, blue);
    }

    void min_five()
    {
        Serial.print("fünf");
        turnLedsOn(18, 21, strip, red, green, blue);
    }

    void hour_six()
    {
        Serial.print("sechs");
        turnLedsOn(56, 60, strip, red, green, blue);
    }

    void hour_seven()
    {
        Serial.print("sieben");
        turnLedsOn(66, 71, strip, red, green, blue);
    }

    void hour_eight()
    {
        Serial.print("acht");
        turnLedsOn(78, 81, strip, red, green, blue);
    }

    void hour_nine()
    {
        Serial.print("neun");
        turnLedsOn(73, 76, strip, red, green, blue);
    }

    void hour_ten()
    {
        Serial.print("zehn");
        turnLedsOn(103, 106, strip, red, green, blue);
    }

    void min_ten()
    {
        Serial.print("zehn");
        turnLedsOn(29, 32, strip, red, green, blue);
    }

    void hour_eleven()
    {
        Serial.print("elf");
        turnLedsOn(107, 109, strip, red, green, blue);
    }

    void hour_twelve()
    {
        Serial.print("zwölf");
        turnLedsOn(93, 97, strip, red, green, blue);
    }

    void quarter()
    {
        Serial.print("viertel");
        turnLedsOn(22, 28, strip, red, green, blue);
    }

    void twenty()
    {
        Serial.print("zwanzig");
        turnLedsOn(11, 17, strip, red, green, blue);
    }

    // ------------------------------------------------------------

    void to()
    {
        Serial.print("vor");
        turnLedsOn(40, 42, strip, red, green, blue);
    }

    void after()
    {
        Serial.print("nach");
        turnLedsOn(35, 38, strip, red, green, blue);
    }

    void half()
    {
        Serial.print("halb");
        turnLedsOn(44, 47, strip, red, green, blue);
    }

    void uhr()
    {
        Serial.print(" uhr");
        turnLedsOn(99, 102, strip, red, green, blue);
    }

} // namespace deutsch