#include <LiquidCrystal.h>

// Tinkercad caveatek listája:
// - A konstansokat (ÉS DEFINEOKAT) nem mindenhol értékeli ki. Például a függvényszignatúrában lévő tömb paraméter méretének cseszik literálon kívül mást elfogadni.
// - Nincsenek templatek!!! Nincs STL!! NINCS SEMMI! KŐBÁNYAI VAN!!!!!
// - Az emulátor nevetségesen lassú, de legalább a belső ideje a helyén van.


// Pinek
const byte PIN_BUTTON_UP = 9;
const byte PIN_BUTTON_DOWN = 8;
const byte PIN_BUTTON_RIGHT = 10;

const byte PIN_LCD_RS = 2;
const byte PIN_LCD_E = 3;
const byte PIN_LCD_DB4 = 4;
const byte PIN_LCD_DB5 = 5;
const byte PIN_LCD_DB6 = 6;
const byte PIN_LCD_DB7 = 7;


// LCD
LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_E, PIN_LCD_DB4, PIN_LCD_DB5, PIN_LCD_DB6, PIN_LCD_DB7);
const byte LCD_WIDTH = 16;
const byte LCD_HEIGHT = 2;
const byte CHAR_WIDTH = 5;
const byte CHAR_HEIGHT = 8;


// Idő
const int FRAMERATE = 25;
const unsigned long MILLIS_PER_FRAME = 1000ul / FRAMERATE;
unsigned long nextFrameDue;

// Debug
#define DEBUG_PRINT_TIMESTAMP() Serial.print('[');\
Serial.print(millis());\
Serial.print(']')

#define DEBUG_PRINT(val) Serial.print(val)
#define DEBUG_PRINTLN(val) Serial.println(val)

#define DEBUG_LOG(val) DEBUG_PRINT_TIMESTAMP();\
DEBUG_PRINT(' ');\
DEBUG_PRINTLN(val);

#define DEBUG_LOG_CAPTIONED(text, val) DEBUG_PRINT_TIMESTAMP();\
DEBUG_PRINT(' ');\
DEBUG_PRINT(text);\
DEBUG_PRINTLN(val);


namespace fire {

    byte mask1[CHAR_HEIGHT] = {
        B00000,
        B00000,
        B00000,
        B00000,
        B00000,
        B00000,
        B00000,
        B00000
    };

    byte mask2[CHAR_HEIGHT] = {
        B00000,
        B00000,
        B00000,
        B00000,
        B00000,
        B00000,
        B00000,
        B00000
    }; 

    const byte SPEED1 = 1;
    const byte SPEED2 = 3;
    const byte PERIOD = SPEED1 * SPEED2 * CHAR_WIDTH; // Igazából lcm kéne az összeszorzás helyett.
    byte phase = 0;


    void fillMask(byte mask[8 /* CHAR_HEIGHT, csak a Tinkercad valamiért nem szereti */]) {
        for(size_t i = 0; i < CHAR_HEIGHT; i++) {
            byte row = 0;
            for(int j = 0; j < i + 1; j++) {
                row |= (B1 << random(CHAR_WIDTH));
            }
            mask[i] = row;
        }
    }

    byte rotateCharRow(byte row, byte shift) {
        shift %= CHAR_WIDTH;
        if(shift < 0) shift = CHAR_WIDTH - shift;
        row = (row << shift) | (row >> (CHAR_WIDTH - shift));
        return row;
    }

    void compose(byte dest[8 /* CHAR_HEIGHT */]) {
        for(size_t i = 0; i < CHAR_HEIGHT; i++) {
            dest[i] = rotateCharRow(mask1[i], phase / SPEED1)
                & rotateCharRow(mask2[i], -(phase / SPEED2));
        }
    }

    void advancePhase() {
        phase = (phase + 1) % PERIOD;
    }

}


void setup() {
    Serial.begin(9600);
    lcd.begin(16, 2);
    DEBUG_LOG("LCD begin");

    pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
    pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);
    pinMode(PIN_BUTTON_RIGHT, INPUT_PULLUP);

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(PIN_LCD_RS, OUTPUT);
    pinMode(PIN_LCD_E, OUTPUT);
    pinMode(PIN_LCD_DB4, OUTPUT);
    pinMode(PIN_LCD_DB5, OUTPUT);
    pinMode(PIN_LCD_DB6, OUTPUT);
    pinMode(PIN_LCD_DB7, OUTPUT);
    DEBUG_LOG("Pin modes set");

    fire::fillMask(fire::mask1);
    fire::fillMask(fire::mask2);
    DEBUG_LOG("Fire filled");

    DEBUG_LOG_CAPTIONED("Frame length: ", MILLIS_PER_FRAME);
    nextFrameDue = millis() + MILLIS_PER_FRAME;
}

void loop() {
    bool up = (digitalRead(PIN_BUTTON_UP) == LOW);
    bool down = (digitalRead(PIN_BUTTON_DOWN) == LOW);
    bool right = (digitalRead(PIN_BUTTON_RIGHT) == LOW);

    byte fire[CHAR_HEIGHT];
    fire::advancePhase();
    fire::compose(fire);
    lcd.createChar(0, fire);

    // A képernyőt feltölteni 25ms (a 40-ből!!!)
    for(int y = 0; y < LCD_HEIGHT; y++) {
        lcd.setCursor(0, y);
        for(int x = 0; x < LCD_WIDTH; x++) {
            lcd.write(byte(0));
        }
    }

    lcd.setCursor(2, 0);
    if(up) lcd.print(" Up");
    if(down) lcd.print(" Down");
    if(right) lcd.print(" Right");

    // Kitaláljuk, mennyit kell várni a következő frame elejéig
    unsigned long now = millis();
    if(now < nextFrameDue) {
        unsigned long slack = nextFrameDue - now;
        delay(nextFrameDue - now);
        DEBUG_LOG_CAPTIONED("Slack: ", slack);
    } else {
        DEBUG_LOG_CAPTIONED("Lag: ", now - nextFrameDue);
    }
    nextFrameDue += MILLIS_PER_FRAME;
}
