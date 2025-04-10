#include <LiquidCrystal.h>

// Tinkercad caveatek listája:
// - A konstansokat (ÉS DEFINEOKAT) nem mindenhol értékeli ki. Például a függvényszignatúrában lévő tömb paraméter méretének cseszik literálon kívül mást elfogadni.


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
#define CHAR_WIDTH 5
#define CHAR_HEIGHT 8


byte FIRE_MASK1[CHAR_HEIGHT] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};

byte FIRE_MASK2[CHAR_HEIGHT] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};

void fillFireMask(byte mask[8 /* CHAR_HEIGHT, csak a Tinkercad valamiért nem szereti */]) {
    for(size_t i = 0; i < CHAR_HEIGHT; i++) {
        byte row = 0;
        for(int j = 0; j < i + 1; j++) {
            row |= (B1 << random(CHAR_WIDTH));
        }
        mask[i] = row;
    }
}

void setup() {
    lcd.begin(16, 2);

    pinMode(LED_BUILTIN, OUTPUT);

    pinMode(PIN_BUTTON_UP, INPUT);
    pinMode(PIN_BUTTON_DOWN, INPUT);
    pinMode(PIN_BUTTON_RIGHT, INPUT);

    pinMode(PIN_LCD_RS, OUTPUT);
    pinMode(PIN_LCD_E, OUTPUT);
    pinMode(PIN_LCD_DB4, OUTPUT);
    pinMode(PIN_LCD_DB5, OUTPUT);
    pinMode(PIN_LCD_DB6, OUTPUT);
    pinMode(PIN_LCD_DB7, OUTPUT);

    fillFireMask(FIRE_MASK1);
    lcd.createChar(0, FIRE_MASK1);
    lcd.createChar(0, FIRE_MASK2);
    // TODO összeXorozni
}

void loop() {
    bool up = (digitalRead(PIN_BUTTON_UP) == LOW);
    bool down = (digitalRead(PIN_BUTTON_DOWN) == LOW);
    bool right = (digitalRead(PIN_BUTTON_RIGHT) == LOW);

    lcd.clear();
    lcd.write(byte(0));

    if(up) lcd.print("Up ");
    if(down) lcd.print("Down ");
    if(right) lcd.print("Right ");

    delay(100);
}
