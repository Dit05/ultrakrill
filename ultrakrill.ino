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



namespace gfx {

    class Fire {

    public:

        Fire() {
            fillMask(mask1);
            fillMask(mask2);
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

    private:
        const byte SPEED1 = 1;
        const byte SPEED2 = 3;
        const byte PERIOD = SPEED1 * SPEED2 * CHAR_WIDTH; // Igazából lcm kéne az összeszorzás helyett.


        byte phase;

        // Mutábilis tömbök a tűznek, a fillMask tölti fel őket.
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


        byte rotateCharRow(byte row, byte shift) {
            shift %= CHAR_WIDTH;
            if(shift < 0) shift = CHAR_WIDTH - shift;
            row = (row << shift) | (row >> (CHAR_WIDTH - shift));
            return row;
        }

        void fillMask(byte mask[8 /* CHAR_HEIGHT, csak a Tinkercad valamiért nem szereti */]) {
            for(size_t i = 0; i < CHAR_HEIGHT; i++) {
                byte row = 0;
                for(int j = 0; j < i + 1; j++) {
                    row |= (B1 << random(CHAR_WIDTH));
                }
                mask[i] = row;
            }
        }

    };

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
            0b01101,
            0b10110,
            0b01010,
            0b10101,
            0b01001,
            0b11010,
            0b10001,
            0b11110
        };

        const byte PLAYER_WALK1[CHAR_HEIGHT] = {
            0b00000,
            0b00110,
            0b01000,
            0b10110,
            0b11011,
            0b11100,
            0b01111,
            0b01000
        };

        const byte PLAYER_WALK2[CHAR_HEIGHT] = {
            0b00110,
            0b01000,
            0b10110,
            0b11100,
            0b11011,
            0b01100,
            0b11010,
            0b00010
        };

        const byte PLAYER_SLIDE[CHAR_HEIGHT] = {
            0b00000,
            0b00000,
            0b01100,
            0b10000,
            0b01100,
            0b11011,
            0b11100,
            0b01011
        };

        const byte FILTH1[CHAR_HEIGHT] = {
            0b00000,
            0b01110,
            0b11011,
            0b01110,
            0b11011,
            0b11110,
            0b01010,
            0b00010
        };

        const byte FILTH2[CHAR_HEIGHT] = {
            0b00000,
            0b01110,
            0b11011,
            0b01010,
            0b11111,
            0b01111,
            0b01010,
            0b01000
        };

        const byte IMP1[CHAR_HEIGHT] = {
            0b00000,
            0b00110,
            0b01110,
            0b01011,
            0b11111,
            0b00011,
            0b01110,
            0b00000
        };

        const byte IMP2[CHAR_HEIGHT] = {
            0b00000,
            0b00000,
            0b00110,
            0b01111,
            0b01011,
            0b11111,
            0b01110,
            0b00000
        };

        const byte IMP_FIRE[CHAR_HEIGHT] = {
            0b00110,
            0b01001,
            0b11111,
            0b00011,
            0b11000,
            0b11001,
            0b00010,
            0b01100
        };

        /*
        const byte TEMPLATE[CHAR_HEIGHT] = {
            0b00000,
            0b00000,
            0b00000,
            0b00000,
            0b00000,
            0b00000,
            0b00000,
            0b00000
        };
        */

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
                    bool streak = false; // A setCursor kihagyható, ha egymás mellett van a két módosítandó.
                    for(int x = 0; x < LCD_WIDTH; x++) {
                        byte here = *this->index(x, y);
                        if(here != *last_p->index(x, y)) {
                            if(!streak) lcd_p->setCursor(x, y);
                            lcd_p->write(here);
                            streak = true;
                        } else {
                            streak = false;
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

namespace game {

    class Game {
    public:


        Game(::LiquidCrystal* lcd_p) {
            this->lcd_p = lcd_p;
            bufferB.clear();

            initializeCustomChars();
        }


        void setButtons(bool up, bool down, bool right) {
            this->buttonUp = up;
            this->buttonDown = down;
            this->buttonRight = right;
        }

        void process() {
            animationTimer++;
            if(animationTimer % 5 == 0) playerFrame = (playerFrame + 1) % 2;
            if(animationTimer % 10 == 0) filthFrame = (filthFrame + 1) % 2;
            if(animationTimer % 10 == 0) impFrame = (impFrame + 1) % 2;

            fire.advancePhase();
        }

        void initializeCustomChars() {
            // A const sajnos elcastolandó, mert a LiquidCrystal hülye.
            lcd_p->createChar(gfx::CHAR_WALL, (byte*)gfx::sprites::WALL);
            lcd_p->createChar(gfx::CHAR_WALL_CRACKED, (byte*)gfx::sprites::WALL_CRACKED);

            actualPlayerFrame = 255;
            setCustomChars();
        }

        void draw() {
            frame_p->clear();

            if(buttonUp) delay(15);
            if(buttonDown) delay(15);
            if(buttonRight) delay(15);

            *frame_p->index(0, LCD_HEIGHT - 1) = ::gfx::CHAR_PLAYER;
            *frame_p->index(4, LCD_HEIGHT - 1) = ::gfx::CHAR_FIRE;
            *frame_p->index(5, LCD_HEIGHT - 1) = ::gfx::CHAR_WALL;
            *frame_p->index(6, LCD_HEIGHT - 1) = ::gfx::CHAR_WALL_CRACKED;
            *frame_p->index(7, LCD_HEIGHT - 1) = ::gfx::CHAR_FILTH;
            *frame_p->index(8, LCD_HEIGHT - 1) = ::gfx::CHAR_IMP;

            if(animationTimer % 2 == 0) {
                *frame_p->index(4, LCD_HEIGHT - 1) = '*';
            }
            *frame_p->index(2, LCD_HEIGHT - 1) = '*';

            // HUD
            *frame_p->index(LCD_WIDTH - 1, 0) = ::gfx::CHAR_HEALTH;
            *frame_p->index(LCD_WIDTH - 1, LCD_HEIGHT - 1) = '?';

            if(buttonUp) *frame_p->index(0, 0) = 'u';
            if(buttonDown) *frame_p->index(1, 0) = 'd';
            if(buttonRight) *frame_p->index(2, 0) = 'r';

            setCustomChars();
            presentAndSwap();
        }

    private:
        LiquidCrystal* lcd_p;
        gfx::Frame bufferA;
        gfx::Frame bufferB;
        gfx::Frame* frame_p = &bufferA;
        gfx::Frame* backPtr = &bufferB;

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

        const byte TILE_EMPTY = ' ';
        const byte TILE_WALL = gfx::CHAR_WALL;
        const byte TILE_WALL_CRACKED = gfx::CHAR_WALL_CRACKED;
        const byte TILE_FIRE = gfx::CHAR_FIRE;

        bool buttonUp = false;
        bool buttonDown = false;
        bool buttonRight = false;

        gfx::Fire fire = {};

        byte animationTimer = 0;
        byte playerFrame = 0;
        byte actualPlayerFrame = 255;
        byte filthFrame = 0;
        byte actualFilthFrame = 255;
        byte impFrame = 0;
        byte actualImpFrame = 255;

        void setCustomChars() {
            if(actualPlayerFrame != playerFrame) {
                lcd_p->createChar(::gfx::CHAR_PLAYER, (byte*)(playerFrame ? &::gfx::sprites::PLAYER_WALK1 : &::gfx::sprites::PLAYER_WALK2));
                actualPlayerFrame = playerFrame;
            }
            if(actualFilthFrame != filthFrame) {
                lcd_p->createChar(::gfx::CHAR_FILTH, (byte*)(filthFrame ? &::gfx::sprites::FILTH1 : &::gfx::sprites::FILTH2));
                actualFilthFrame = filthFrame;
            }
            if(actualImpFrame != impFrame) {
                lcd_p->createChar(::gfx::CHAR_IMP, (byte*)(impFrame ? &::gfx::sprites::IMP1 : &::gfx::sprites::IMP2));
                actualImpFrame = impFrame;
            }

            byte composedFire[CHAR_HEIGHT];
            fire.compose(composedFire);
            lcd_p->createChar(gfx::CHAR_FIRE, composedFire);
        }

        void presentAndSwap() {
            frame_p->presentDifferential(lcd_p, backPtr);

            gfx::Frame* tmp = frame_p;
            frame_p = backPtr;
            backPtr = tmp;
        }
    };

}



bool lagging = false; // Időtúllépéses-e az előző frame.

game::Game gameInst(&lcd);


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

    DEBUG_LOG_CAPTIONED("Frame length: ", MILLIS_PER_FRAME);
    nextFrameDue = millis() + MILLIS_PER_FRAME;
}

void loop() {
    bool up = (digitalRead(PIN_BUTTON_UP) == LOW);
    bool down = (digitalRead(PIN_BUTTON_DOWN) == LOW);
    bool right = (digitalRead(PIN_BUTTON_RIGHT) == LOW);

    gameInst.setButtons(up, down, right);

    gameInst.process();
    // Ha időhiányban szenvedünk, akkor átugorjuk a kirajzolást, mert ez jelentős időbe telik, és "nem hat" a játékmenetre.
    if(!lagging) {
        gameInst.draw();
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
