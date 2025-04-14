#include <LiquidCrystal.h>

// Tinkercad caveatek listája:
// - A konstansokat (ÉS DEFINEOKAT) nem mindenhol értékeli ki. Például a függvényszignatúrában lévő tömb paraméter méretének cseszik literálon kívül mást elfogadni.
// - Nincsenek templatek!!! Nincs STL!! NINCS SEMMI! KŐBÁNYAI VAN!!!!!
// - Az emulátor nevetségesen lassú, de legalább a belső ideje a helyén van.
// - Nincs enum class!!!
// - Nincs reference!!!
// - Csak akkor értelmezi a classban a struct paraméteres metódust, ha fully qualified az útvonala, tehát NEM JÓ a LiquidCrystal, ::LiquidCrystal kell.

// Konfigurációs DEFINEok
#define CONF_DEBUG_LOGGING 1

#define CONF_PANIC_BOUNDS 1
#define CONF_PANIC_LAG 0


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
#if CONF_DEBUG_LOGGING == 1

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

#else //

#define DEBUG_PRINT_TIMESTAMP()
#define DEBUG_PRINT(val)
#define DEBUG_PRINTLN(val)
#define DEBUG_LOG(val)
#define DEBUG_LOG_CAPTIONED(text, val)

#endif


const int PANIC_FRAME_INDEX_OUT_OF_RANGE = 1;
const int PANIC_LAG = 2;


// Végtelen ciklusban villogja le a megadott `code`-ot.
void panic(byte code) {
    Serial.print("Panic code: ");
    Serial.println(code);

    const int BIT_TIME = 1000;

    byte mask = 0;
    while(true) {
        mask >>= 1;
        if(mask <= 0) {
            mask = 1 << 7;
            delay(BIT_TIME * 2);
        }

        int duty = ((byte)code & mask) ? 500 : 100;

        digitalWrite(LED_BUILTIN, HIGH);
        delay(duty);
        digitalWrite(LED_BUILTIN, LOW);
        delay(BIT_TIME - duty);
    }
}

// Mint a sima, csak még pluszba elküld egy üzenet Serial-on.
void panic(byte code, const char* msg) {
    Serial.print("Panic message: ");
    Serial.println(msg);
    panic(code);
}


namespace fire {

    // Mutábilis tömbök a tűznek, a fillMask tölti fel őket a setupban.
    byte mask1[CHAR_HEIGHT] = {
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000
    };

    byte mask2[CHAR_HEIGHT] = {
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000
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

namespace gfx {

    namespace sprites {

        const byte WALL[CHAR_HEIGHT] = {
            0b11111,
            0b10001,
            0b10001,
            0b10001,
            0b10001,
            0b10001,
            0b10001,
            0b11111
        };

        const byte WALL_CRACKED[CHAR_HEIGHT] = {
            0b00111,
            0b11010,
            0b10010,
            0b10101,
            0b10001,
            0b11010,
            0b10001,
            0b01111
        };

    }


    class Frame {

    public:
        byte* index(int x, int y) {
#if CONF_PANIC_BOUNDS == 1
            if(x < 0 || y < 0 || x >= LCD_WIDTH || y >= LCD_HEIGHT) {
                auto msg = String();
                msg += "Indexing Frame out of range with x=";
                msg += x;
                msg += ", y=";
                msg += y;
                panic(PANIC_FRAME_INDEX_OUT_OF_RANGE, msg.c_str());
            }
#endif
            return &buffer[(y * LCD_WIDTH) + x]; // A klasszikus 2D tömb indexelős képlet.
        }

        void clear(char ch) {
            for(int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) buffer[i] = ch;
        }

        void clear() { clear(' '); }


        // Kirajzolja ezt a framet az egész LCD tartalmának felülírásával.
        void present(::LiquidCrystal* lcd_p) {
            for(int y = 0; y < LCD_HEIGHT; y++) {
                lcd_p->setCursor(0, y);
                for(int x = 0; x < LCD_WIDTH; x++) {
                    lcd_p->write(*index(x, y));
                }
            }
        }

        // Kirajzolja ezt a Framet egy már fentlévőre.
        void presentDifferential(::LiquidCrystal* lcd_p, gfx::Frame* last_p) {
            int differences = 0; // Vajon a compiler csinál ebből byte-t?
            for(int y = 0; y < LCD_HEIGHT; y++) {
                for(int x = 0; x < LCD_WIDTH; x++) {
                    if(*this->index(x, y) != *last_p->index(x, y)) differences++;
                }
            }

            // Legalább a fele különbözik?
            if(differences * 2 >= LCD_WIDTH * LCD_HEIGHT) {
                this->present(lcd_p); // Akkor csak present-eld simán.
            } else {
                // Kevés különbség, mehetünk esetenként.
                for(int y = 0; y < LCD_HEIGHT; y++) {
                    for(int x = 0; x < LCD_WIDTH; x++) {
                        byte here = *this->index(x, y);
                        if(here != *last_p->index(x, y)) {
                            lcd_p->setCursor(x, y);
                            lcd_p->write(here);
                        }
                    }
                }
            }
        }

    private:
        byte buffer[LCD_WIDTH * LCD_HEIGHT];

    };


    const byte CHAR_FIRE = 0; // Procedural fire
    const byte CHAR_PLAYER = 1; // Animated player character
    const byte CHAR_WALL = 2; // Pristine wall
    const byte CHAR_WALL_CRACKED = 3; // Damaged wall
    const byte CHAR_HEALTH = 4; // Procedural health bar
    const byte CHAR_FILTH = 5; // Animated character for the ground enemy.
    const byte CHAR_IMP = 6; // Animated character for the flying-shooting enemy.
    const byte CHAR_UNUSED7 = 7;

}



gfx::Frame frontBuffer;
gfx::Frame backBuffer;
gfx::Frame* frontPtr = &frontBuffer;
gfx::Frame* backPtr = &backBuffer;

// Ezt töltögetjük majd fel annyira, hogy az életerőt mutassa.
byte healthChar[CHAR_HEIGHT] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000
};

bool lagging = false; // Időtúllépéses-e az előző frame.


void setup() {
    Serial.begin(9600);
    lcd.begin(LCD_WIDTH, LCD_HEIGHT);
    DEBUG_LOG("LCD began");

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

    frontBuffer.clear();
    backBuffer.clear();
}

void loop() {
    bool up = (digitalRead(PIN_BUTTON_UP) == LOW);
    bool down = (digitalRead(PIN_BUTTON_DOWN) == LOW);
    bool right = (digitalRead(PIN_BUTTON_RIGHT) == LOW);

    byte fire[CHAR_HEIGHT];
    fire::advancePhase();
    fire::compose(fire);
    lcd.createChar(0, fire);


    // Demo: ' ' -> *, * -> (tűz).
    while(random(3) != 0) {
        byte* ch = frontPtr->index(random(LCD_WIDTH), random(LCD_HEIGHT));
        if(*ch != '*') {
            *ch = '*';
        } else {
            *ch = 0;
        }
    }

    if(up) delay(MILLIS_PER_FRAME); // Lag gomb


    // Ha időhiányban szenvedünk, akkor átugorjuk a kirajzolást, mert ez jelentős időbe telik, és "nem hat" a játékmenetre.
    if(!lagging) {
        frontPtr->presentDifferential(&lcd, backPtr);

        *backPtr = *frontPtr; // Átmásolás (mert nem töltjük ki a teljes front buffert)

        // Swap
        gfx::Frame* tmp = frontPtr;
        frontPtr = backPtr;
        backPtr = tmp;
    }

    // Kitaláljuk, mennyit kell várni a következő frame elejéig
    unsigned long now = millis();
    lagging = now > nextFrameDue;
    if(!lagging) {
        unsigned long slack = nextFrameDue - now;

        digitalWrite(LED_BUILTIN, HIGH);
        delay(nextFrameDue - now);
        digitalWrite(LED_BUILTIN, LOW);

        DEBUG_LOG_CAPTIONED("Slack: ", slack);
    } else {
        DEBUG_LOG_CAPTIONED("Lag: ", now - nextFrameDue);

#if CONF_PANIC_LAG
        panic(PANIC_LAG, "We are lagging :(");
#endif
    }
    nextFrameDue += MILLIS_PER_FRAME;
}
