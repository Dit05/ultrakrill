#include <new>
#ifndef LCD_EMULATOR
#include <LiquidCrystal.h>
#endif

// Tinkercad caveatek listája:
// - A konstansokat (ÉS DEFINEOKAT) nem mindenhol értékeli ki. Például a függvényszignatúrában lévő tömb paraméter méretének cseszik literálon kívül mást elfogadni.
// - Nincsenek templatek!!! Nincs STL!! NINCS SEMMI! KŐBÁNYAI VAN!!!!!
// - Az emulátor nevetségesen lassú, de legalább a belső ideje a helyén van.
// - Nincs enum class!!!
// - Nincs reference!!!
// - Csak akkor értelmezi a classban a struct paraméteres metódust, ha fully qualified az útvonala, tehát NEM JÓ a LiquidCrystal, ::LiquidCrystal kell.
// - A virtual method csak úgy parsolódik helyesen, ha {} helyett = 0;.
// - invalid header file = nem is invalid, csak van valami egyéb hiba amit nem mond el, és includeolva van az EEPROM.h.
// - Bizonyos sketch méret fölött egyszerűen nem hajlandó elhinni, hogy ráfér a flashre... (utolsó óceán a pohárban)
// TODO a valóságban változik az LCD, ha csak custom characterek definíciói változtak, de nem volt semmi write?

// Konfigurációs DEFINEok
#define CONF_DEBUG_LOGGING 1
#define CONF_DEBUG_ALWAYS_LOG_TIME 0
#define CONF_DEBUG_LOG_PUSH_FAILURES 1
#define CONF_DEBUG_LOG_ALLOCS 1

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
bool lagging = false; // Időtúllépéses-e az előző frame.


// Scenek
namespace game { struct Stats; }
void switchToMenu();
void switchToIntro();
void switchToGame(uint16_t seed);
void switchToSkull(game::Stats stats);


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

#if CONF_DEBUG_LOG_ALLOCS

#define LOG_NEW(what) DEBUG_LOG_CAPTIONED(F("new "), (unsigned int)what)
#define LOG_NEWS(what) DEBUG_LOG_CAPTIONED(F("operator new[] "), (unsigned int)what)
#define LOG_DELETE(what) DEBUG_LOG_CAPTIONED(F("delete "), (unsigned int)what)
#define LOG_DELETES(what) DEBUG_LOG_CAPTIONED(F("delete[] "), (unsigned int)what)

#else //

#endif


const byte PANIC_FRAME_INDEX_OUT_OF_RANGE = 1;
const byte PANIC_COLLECTION_INDEX_OUT_OF_RANGE = 2;
const byte PANIC_COLLECTION_FULL = 3;
const byte PANIC_COLLECTION_EMPTY = 4;
const byte PANIC_LAG = 5;
const byte PANIC_INVALID_ENUM = 6;
const byte PANIC_BLOCKMAP_INDEX_OUT_OF_RANGE = 7;
const byte PANIC_ALLOCATION_FAILED = 8;
const byte PANIC_SCROLLER_INDEX_OUT_OF_RANGE = 9;


// Végtelen ciklusban villogja le a megadott `code`-ot.
void panic(byte code) {
    Serial.print(F("Panic code: "));
    Serial.println(code);

#ifdef LCD_EMULATOR
    *(int*)0 = 149; // DAP Breakpoint
#endif

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
template<typename T>
void panic(byte code, const T msg) {
    Serial.print(F("Panic message: "));
    Serial.println(msg);
    panic(code);
}



// Hacky generikus vektor
template<typename T, unsigned int CAP>
class Vec {
public:
    static const byte CAPACITY = CAP;

    byte size() const { return size_m; }
    T* operator [](byte index) {
        checkBounds(index);
        return &((T*)buffer)[index];
    }

    void removeAt(byte index) {
        checkBounds(index);

        size_m--;
        for(int i = index; i < size_m; i++) {
            ((T*)buffer)[i] = ((T*)buffer)[i + 1];
        }
    }

    bool tryPush(T elem) {
        if(size_m >= CAP) return false;
        ((T*)buffer)[size_m] = elem;
        size_m++;
        return true;
    }

    void push(T elem) {
        if(!tryPush(elem)) panic(PANIC_COLLECTION_FULL);
    }

    // Returns whether pushing was done forcefuly.
    bool forcePush(T elem) {
        if(size_m >= CAP) {
            ((T*)buffer)[size_m] = elem;
            return true;
        } else {
            ((T*)buffer)[size_m] = elem;
            size_m++;
            return false;
        }
    }

    T pop() {
        if(size_m <= 0) panic(PANIC_COLLECTION_EMPTY);
        size_m--;
        return ((T*)buffer)[size_m];
    }

private:
    byte size_m = 0;
    byte buffer[CAP * sizeof(T)] {};

    void checkBounds(byte index) const {
        if(index < 0 || index >= size_m) panic(PANIC_COLLECTION_INDEX_OUT_OF_RANGE);
    }
};


template<typename T>
class List {

public:
    unsigned int capacity() const { return capacity_m; }
    unsigned int size() const { return size_m; }
    T* rawPtr() const { return ptr_m; }


    List(unsigned int capacity) {
        capacity_m = 0;
        size_m = 0;
        ptr_m = NULL;
        resize(capacity);
    }

    List() {
        capacity_m = 0;
        size_m = 0;
        ptr_m = NULL;
        resize(4);
    }

    ~List() {
        Serial.println("list destructor");
        if(ptr_m != NULL) {
            LOG_DELETES(ptr_m);
            delete[] ptr_m;
        }
    }


    void resize(unsigned int newCapacity) {
        T* old_ptr = ptr_m;

        unsigned int size = newCapacity * sizeof(T);
        DEBUG_LOG_CAPTIONED(F("List realloc "), size);
        ptr_m = (T*)operator new[](size);
        LOG_NEWS(ptr_m);
        if(ptr_m == NULL) {
            ptr_m = old_ptr;
            panic(PANIC_ALLOCATION_FAILED, F("List"));
            return;
        }

        memcpy(ptr_m, old_ptr, sizeof(T) * min(capacity_m, newCapacity));
        capacity_m = newCapacity;

        LOG_DELETES(old_ptr);
        delete[] old_ptr;
    }

    void reserve(unsigned int capacity) {
        unsigned int newCap = capacity_m;
        while(newCap < capacity) newCap *= 2;
        if(newCap != capacity_m) resize(newCap);
    }

    void shrinkToFit() {
        resize(size_m);
    }


    T* operator[](unsigned int index) const {
        checkBounds(index);
        return &ptr_m[index];
    }


    void push(T elem) {
        if(size_m >= capacity_m) resize(capacity_m * 2);
        ptr_m[size_m++] = elem;
    }

    T pop() {
        if(size_m <= 0) panic(PANIC_COLLECTION_EMPTY);
        return ptr_m[--size_m];
    }

    void removeAt(byte index) {
        checkBounds(index);

        size_m--;
        for(unsigned int i = index; i < size_m; i++) {
            ptr_m[i] = ptr_m[i + 1];
        }
    }


private:
    T* ptr_m;
    unsigned int capacity_m;
    unsigned int size_m;

    void checkBounds(unsigned int index) const {
        if(index < 0 || index >= size_m) panic(PANIC_COLLECTION_INDEX_OUT_OF_RANGE);
    }

};


class StringWrapper {
public:
    virtual char operator[](unsigned int i) const = 0;
};

class CStringWrapper final : public StringWrapper {
public:
    CStringWrapper(const char* str) : str_m(str) {}

    char operator[](unsigned int i) const {
        return str_m[i];
    }

private:
    const char* str_m;
};

class FlashStringWrapper final : public StringWrapper {
public:
    FlashStringWrapper(const __FlashStringHelper* str) : str_m((const char PROGMEM *)str) {}
    FlashStringWrapper(const char* PROGMEM str) : str_m(str) {}

    char operator[](unsigned int i) const {
        return pgm_read_byte(&str_m[i]);
    }

private:
    const char* PROGMEM str_m;
};


// https://en.wikipedia.org/wiki/Linear-feedback_shift_register nyomán.
class Random {

public:
    Random(uint16_t startState) {
        if(startState == 0) startState++;
        lfsr = startState;
        warmup(20); // "Bemelegítés: 20 fekvőtámasz"
    }


    void reseed(uint16_t seed) {
        if(seed == 0) seed++;
        lfsr = seed;
    }

    void warmup(unsigned int steps) {
        while(steps --> 0) nextBit();
    }

    bool nextBit() {
        return advance();
    }

    byte nextUint8() {
        byte val = 0;
        for(byte i = 0; i < 8; i++) val |= ((byte)nextBit() << i);
        return val;
    }

    byte nextUint8(byte maxExcl) {
        if(maxExcl <= 0) return 0;

        while(true) {
            byte val = 0;
            for(byte i = 0; i < width(maxExcl); i++) val |= ((byte)nextBit() << i);
            if(val < maxExcl) return val;
        }
    }

    uint16_t nextUint16() {
        uint16_t val = 0;
        for(byte i = 0; i < 16; i++) val |= ((uint16_t)nextBit() << i);
        return val;
    }

    uint16_t nextUint16(uint16_t maxExcl) {
        if(maxExcl <= 0) return 0;

        while(true) {
            uint16_t val = 0;
            for(byte i = 0; i < width(maxExcl); i++) val |= ((byte)nextBit() << i);
            if(val < maxExcl) return val;
        }
    }

    uint32_t nextUint32() {
        uint32_t val = 0;
        for(byte i = 0; i < 32; i++) val |= ((uint32_t)nextBit() << i);
        return val;
    }

    uint32_t nextUint32(uint32_t maxExcl) {
        if(maxExcl <= 0) return 0;

        while(true) {
            uint32_t val = 0;
            for(byte i = 0; i < width(maxExcl); i++) val |= ((byte)nextBit() << i);
            if(val < maxExcl) return val;
        }
    }

private:
    uint16_t lfsr;

    bool advance() {
        // Csapok: 16, 14, 13, 11 (köszönöm Wikipédia, nagyon menő)
        uint16_t bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
        lfsr = (lfsr >> 1) | (bit << 15);
        return bit;
    }

    static byte width(uint8_t val) {
        byte log = 0;
        while(val > 0) { log++; val >>= 1; }
        return log;
    }
    static byte width(uint16_t val) {
        byte log = 0;
        while(val > 0) { log++; val >>= 1; }
        return log;
    }
    static byte width(uint32_t val) {
        byte log = 0;
        while(val > 0) { log++; val >>= 1; }
        return log;
    }
};

struct Buttons {
    bool up;
    bool down;
    bool right;

    Buttons operator !() {
        return Buttons {
            .up = !up,
            .down = !down,
            .right = !right
        };
    }

    Buttons operator ||(Buttons others) {
        return Buttons {
            .up = up || others.up,
            .down = down || others.down,
            .right = right || others.right
        };
    }

    Buttons operator &&(Buttons others) {
        return Buttons {
            .up = up && others.up,
            .down = down && others.down,
            .right = right && others.right
        };
    }

    Buttons operator ^(Buttons others) {
        return Buttons {
            .up = bool(up ^ others.up),
            .down = bool(down ^ others.down),
            .right = bool(right ^ others.right)
        };
    }
};

class Scene {

public:
    virtual void setInputs(Buttons held, Buttons pressed, Buttons released) = 0;
    virtual void process() = 0;
    virtual void draw(::LiquidCrystal* lcd_p) = 0;
    virtual void suspend() = 0;
    virtual void resume(::LiquidCrystal* lcd_p) = 0;
    virtual ~Scene() {}

};


namespace text {
    // CSAK ÍGY MŰKÖDIK A PROGMEM!! (végre rájöttem)
    const char INTRO_F[] PROGMEM = {"MACHINE ID:\n V1\nSTATUS:\nAPPROACHING HELL\n\nMANKIND IS DEAD.\nBLOOD IS FUEL.\nHELL IS FULL.\n"};
}


namespace gfx {

    const char* const HEX_DIGITS = "0123456789ABCDEF";

    constexpr const byte CHAR_FIRE = 0; // Procedural fire.
    constexpr const byte CHAR_PLAYER = 1; // Animated player character.
    constexpr const byte CHAR_WALL = 2; // Pristine wall.
    constexpr const byte CHAR_CRACKED_WALL = 3; // Damaged wall.
    constexpr const byte CHAR_HEALTH = 4; // Procedural health bar.
    constexpr const byte CHAR_FILTH = 5; // Animated character for the ground enemy.
    constexpr const byte CHAR_IMP = 6; // Animated character for the flying-shooting enemy.
    constexpr const byte CHAR_OVERLAY = 7; // Character for full-screen overlay.

    const char* const SHOOT_CHARGE_CHARS_F = " .,;|+*";

    const char* SMOKE = ",;x&@";
    const byte SMOKE_MAX = strlen(SMOKE) - 1;

    const char BLOOD = '#';

    const char* DIE_PHRASES[] {
        "INSUFFICIENT",
        "BLOOD",
        "ERROR",
        "FAULT",
        "FAILURE",
        "SYSTEM"
    };
    const byte DIE_PHRASE_COUNT = sizeof(DIE_PHRASES) / sizeof(char*);


    class Fire {

    public:

        Fire() {
            ::Random rand(millis() + 149);
            randomizeMasks(&rand);
        }

        Fire(::Random* rand_p) {
            randomizeMasks(rand_p);
        }


        void randomizeMasks(::Random* rand_p) {
            fillMask(rand_p, mask1);
            fillMask(rand_p, mask2);
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

        void fillMask(::Random* rand_p, byte mask[8]) {
            for(byte i = 0; i < CHAR_HEIGHT; i++) {
                byte row = 0;
                for(byte j = 0; j < i + 1; j++) {
                    row |= (0b1 << rand_p->nextUint8(CHAR_WIDTH));
                }
                mask[i] = row;
            }
        }

    };

    namespace sprites {

        void flip(byte* sprite) {
            for(byte i = 0; i < CHAR_HEIGHT; i++) {
                byte row = sprite[i];
                byte flipped = 0;
                for(byte j = 0; j < CHAR_WIDTH; j++) {
                    byte right = (row >> j) & 1;
                    byte left = (row >> (CHAR_WIDTH - 1 - j)) & 1;
                    flipped |= (left << j) | (right << (CHAR_WIDTH - 1 - j));
                }
                sprite[i] = flipped;
            }
        }

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

        const byte CRACKED_WALL[CHAR_HEIGHT] = {
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

        const byte FLASH0[CHAR_HEIGHT] = {
            0b10001,
            0b00100,
            0b10001,
            0b00100,
            0b10001,
            0b00100,
            0b10001,
            0b00100
        };

        const byte FLASH1[CHAR_HEIGHT] = {
            0b10101,
            0b01010,
            0b10101,
            0b01010,
            0b10101,
            0b01010,
            0b10101,
            0b01010
        };

        const byte FLASH2[CHAR_HEIGHT] = {
            0b11011,
            0b01110,
            0b11011,
            0b01110,
            0b11011,
            0b01110,
            0b11011,
            0b01110
        };

        const byte FLASH3[CHAR_HEIGHT] = {
            0b11111,
            0b11111,
            0b11111,
            0b11111,
            0b11111,
            0b11111,
            0b11111,
            0b11111
        };

        const byte FLASH_FRAMES = 4;


        const byte CROSSBONE_TOP[CHAR_HEIGHT] = {
            0b00000,
            0b00000,
            0b00000,
            0b00000,
            0b00001,
            0b00001,
            0b00001,
            0b00001
        };

        const byte CROSSBONE_BOTTOM[CHAR_HEIGHT] = {
            0b01100,
            0b11110,
            0b00011,
            0b00000,
            0b10011,
            0b01111,
            0b01100,
            0b10000
        };


        const byte SKULL1_TOP[CHAR_HEIGHT] = {
            0b00000,
            0b01111,
            0b11111,
            0b11111,
            0b10011,
            0b10001,
            0b10001,
            0b11111
        };

        const byte SKULL1_BOTTOM[CHAR_HEIGHT] = {
            0b11111,
            0b01110,
            0b00111,
            0b01010,
            0b01111,
            0b00111,
            0b00000,
            0b00000
        };


        const byte SKULL2_TOP[CHAR_HEIGHT] = {
            0b00000,
            0b00000,
            0b01111,
            0b10011,
            0b10001,
            0b10001,
            0b11111,
            0b11111
        };

        const byte SKULL2_BOTTOM[CHAR_HEIGHT] = {
            0b11110,
            0b00111,
            0b01010,
            0b00000,
            0b01010,
            0b01111,
            0b00111,
            0b00000
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
        byte* index(byte x, byte y) {
#if CONF_PANIC_BOUNDS == 1
            if(x < 0 || y < 0 || x >= LCD_WIDTH || y >= LCD_HEIGHT) {
                auto msg = String();
                msg += F("Indexing Frame out of range with x=");
                msg += x;
                msg += F(", y=");
                msg += y;
                panic(PANIC_FRAME_INDEX_OUT_OF_RANGE, msg.c_str());
            }
#endif
            return &buffer[(y * LCD_WIDTH) + x]; // A klasszikus 2D tömb indexelős képlet.
        }

        void write(byte x, byte y, const StringWrapper* text) {
            unsigned int i = 0;
            while(x < LCD_WIDTH && (*text)[i] != '\0') *index(x++, y) = (*text)[i++];
        }
        void write(byte x, byte y, const char* text) {
            while(x < LCD_WIDTH && *text != '\0') *index(x++, y) = *text++;
        }

        void writeRtl(byte x, byte y, unsigned long num) {
            do {
                *index(x--, y) = "0123456789"[num % 10];
                num /= 10;
            } while(x > 0 && num != 0);
        }

        void clear(char ch) {
            for(unsigned int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) buffer[i] = ch;
        }

        void clear() { clear(' '); }


        void scrollUp() {
            for(byte y = 0; y < LCD_HEIGHT - 1; y++) {
                for(byte x = 0; x < LCD_WIDTH; x++) {
                    *index(x, y) = *index(x, y + 1);
                }
            }

            for(byte x = 0; x < LCD_WIDTH; x++) {
                *index(x, LCD_HEIGHT - 1) = ' ';
            }
        }


        // Kirajzolja ezt a framet az egész LCD tartalmának felülírásával.
        void present(::LiquidCrystal* lcd_p) {
            for(byte y = 0; y < LCD_HEIGHT; y++) {
                lcd_p->setCursor(0, y);
                for(byte x = 0; x < LCD_WIDTH; x++) {
                    lcd_p->write(*index(x, y));
                }
            }
        }

        // Kirajzolja ezt a Framet egy már fentlévőre.
        void presentDifferential(::LiquidCrystal* lcd_p, gfx::Frame* last_p) {
            unsigned int differences = 0;
            for(byte y = 0; y < LCD_HEIGHT; y++) {
                for(byte x = 0; x < LCD_WIDTH; x++) {
                    if(*this->index(x, y) != *last_p->index(x, y)) differences++;
                }
            }

            if(differences == 0) {
                // Ha nem változik semmi, akkor is legyen valami, nehogy ne updateljen az LCD.
                lcd_p->setCursor(0, 0);
                lcd_p->write(*this->index(0, 0));
            } else if(differences * 2 >= LCD_WIDTH * LCD_HEIGHT) {
                // Legalább a fele különbözik
                this->present(lcd_p); // Akkor csak present-eld simán.
            } else {
                // Kevés különbség, mehetünk esetenként.
                for(byte y = 0; y < LCD_HEIGHT; y++) {
                    bool streak = false; // A setCursor kihagyható, ha egymás mellett van a két módosítandó.
                    for(byte x = 0; x < LCD_WIDTH; x++) {
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


    class CharViewer : public ::Scene {
    public:
        void setInputs(::Buttons held, ::Buttons pressed, ::Buttons released) {
            this->pressed = pressed;
        }

        void process() {
            for(int i = 0; i < LCD_HEIGHT; i++) if(pressed.up && offset != 0) offset--;
            for(int i = 0; i < LCD_HEIGHT; i++) if(pressed.down && offset + LCD_HEIGHT < 256 / 8) offset++;
        }

        void draw(::LiquidCrystal* lcd_p) {
            for(int y = 0; y < LCD_HEIGHT; y++) {
                int start = (y + offset) * 8;
                lcd_p->setCursor(0, y);
                lcd_p->write(::gfx::HEX_DIGITS[(start & 0xF0) >> 4]);
                lcd_p->write(::gfx::HEX_DIGITS[start & 0x0F]);
                lcd_p->write('|');

                for(int x = 0; x < 8; x++) {
                    lcd_p->write(start + x);
                }
            }
        }

        void suspend() { /* do nothing */ }
        void resume(::LiquidCrystal* lcd_p) {
            lcd_p->clear();
        }

    private:
        Buttons pressed;
        byte offset;
    };

}

namespace game {

    struct Entity {
        byte posX;
        byte posY;
    };

    struct Shot : public Entity {
        byte flags;

        Shot(byte posX, byte posY, bool friendly, bool explosive) {
            this->posX = posX;
            this->posY = posY;
            flags = 0;
            if(friendly) flags |= 1;
            if(explosive) flags |= 2;
        }


        bool friendly() { return (flags & 1) > 0; }
        bool explosive() { return (flags & 2) > 0; }
        void setFriendly(bool val) {
            if(val) flags |= 1;
            else flags &= ~1;
        }
        void setExplosive(bool val) {
            if(val) flags |= 2;
            else flags &= ~2;
        }
    };

    struct Filth : public Entity {
        Filth(byte posX, byte posY) {
            this->posX = posX;
            this->posY = posY;
        }
    };

    struct Imp : public Entity {
        byte health = 3;

        Imp(byte posX, byte posY) {
            this->posX = posX;
            this->posY = posY;
        }
    };


    enum ObstacleKind : byte {
        OBSTACLE_EMPTY = 0,
        OBSTACLE_CRACKED_WALL = 1,
        OBSTACLE_WALL = 2,
        OBSTACLE_FIRE = 3
    };


    enum TileEntity : byte {
        TILE_ENTITY_FIRE = 0,
        TILE_ENTITY_CRACKED_WALL = 1,
        TILE_ENTITY_WALL = 2,
        TILE_ENTITY_CRACKED_WALL_OR_WALL = 3,
        TILE_ENTITY_CRACKED_WALL_OR_FIRE = 4,
        TILE_ENTITY_FILTH = 5,
        TILE_ENTITY_IMP = 6,
        TILE_ENTITY_IMP_OR_FILTH = 7
    };

    // 0: lent vagy fent
    // 1-3: mi
    //  0: tűz
    //  1: repedt fal
    //  2: teljes fal
    //  3: repedt/teljes fal (véletlen)
    //  4: repedt fal/tűz (véletlen)
    //  5: filth
    //  6: imp
    //  7: imp/filth (véletlen)
    // 4-6: kurzor mozgatások száma (0-7)
    // 7: opcionális-e (100% helyett csak 50% eséllyel tevődik le)
    class Tile {

    public:
        static const byte TOP_INDEX = 0;
        static const byte WHAT_OFFSET = 1;
        static const byte WHAT_LEN = 3;
        static const byte MOVES_OFFSET = 4;
        static const byte MOVES_LEN = 3;
        static const byte OPTIONAL_INDEX = 7;

        bool isTop() const { return getBit(TOP_INDEX); }
        game::TileEntity getWhat() const { return (TileEntity)getField(WHAT_OFFSET, WHAT_LEN); }
        byte getMoves() const { return getField(MOVES_OFFSET, MOVES_LEN); }
        bool isOptional() const { return getBit(OPTIONAL_INDEX); }

        // 1 kurzormozgatásos opcionálatlan tűz lent.
        constexpr Tile() : data(MOVES_OFFSET) { /* do nothing, különben nem "compile-time constant" */ }

        // Csak haladóknak ajánlott ez a konstruktor!
        constexpr Tile(byte data) : data(data) { /* do nothing */ }


        ::game::Tile bottom() const { return withBit(TOP_INDEX, false); }
        ::game::Tile top() const { return withBit(TOP_INDEX, true); }

        ::game::Tile what(game::TileEntity what) const { return withField(WHAT_OFFSET, WHAT_LEN, what); }

        ::game::Tile moves(byte moves) const { return withField(MOVES_OFFSET, MOVES_LEN, moves); }

        ::game::Tile mandatory() const { return withBit(OPTIONAL_INDEX, false); }
        ::game::Tile optional() const { return withBit(OPTIONAL_INDEX, true); }

        String toString() {
            String str {};

            if(isOptional()) str += F("optional ");
            if(isTop()) str += F("top ");
            else str += F("bottom ");

            switch(getWhat()) {
                case TILE_ENTITY_FIRE: str += F("fire"); break;
                case TILE_ENTITY_CRACKED_WALL: str += F("cracked wall"); break;
                case TILE_ENTITY_WALL: str += F("wall"); break;
                case TILE_ENTITY_CRACKED_WALL_OR_WALL: str += F("cracked/regular wall"); break;
                case TILE_ENTITY_CRACKED_WALL_OR_FIRE: str += F("cracked wall/fire"); break;
                case TILE_ENTITY_FILTH: str += F("filth"); break;
                case TILE_ENTITY_IMP: str += F("imp"); break;
                case TILE_ENTITY_IMP_OR_FILTH: str += F("imp/filth"); break;
                default: str += F("invalid value"); break;
            }

            str += F(", moves: ");
            str += getMoves();

            return str;
        }


    private:
        byte data;

        constexpr bool getBit(byte index) const {
            return (data & (1 << index)) > 0;
        }

        constexpr byte getField(byte offset, byte len) const {
            return (data >> offset) & (0xff >> (8 - len));
        }

        ::game::Tile withBit(byte index, bool val) const {
            byte newData = data;
            byte mask = 1 << index;
            newData &= ~mask;
            if(val) newData |= mask;
            return Tile(newData);
        }

        ::game::Tile withField(byte offset, byte len, byte val) const {
            byte newData = data;
            byte mask = 0xff >> (8 - len);
            newData &= ~(mask << offset);
            newData |= (val & mask) << offset;
            return Tile(newData);
        }

    };

    struct Pattern {
        byte tileCount;
        Tile* tiles;
    };


    class PatternArray {
    public:
        void add(const char* top_p, const char* bottom_p) {
            offsets.push(tiles.size());
            while(*top_p != '\0' && *bottom_p != '\0') {
                if(*top_p != ' ') {
                    Tile top = decodeTile(*top_p).top();

                    if(*bottom_p == ' ') top = top.moves(untilNextColumn(top_p, bottom_p));
                    tiles.push(top);
                }
                if(*bottom_p != ' ') {
                    Tile bottom = decodeTile(*bottom_p).bottom().moves(untilNextColumn(top_p, bottom_p));
                    tiles.push(bottom);
                }

                top_p++;
                bottom_p++;
            }
        }

        void mark() {
            marks.push(offsets.size());
        }

        game::TileEntity decodeEntity(char ch) {
            switch(ch) {
                case 'w': case 'W': return TILE_ENTITY_FIRE;
                case 'x': case 'X': return TILE_ENTITY_WALL;
                case 'y': case 'Y': return TILE_ENTITY_CRACKED_WALL;
                case 'z': case 'Z': return TILE_ENTITY_CRACKED_WALL_OR_WALL;
                case 'v': case 'V': return TILE_ENTITY_CRACKED_WALL_OR_FIRE;
                case 'f': case 'F': return TILE_ENTITY_FILTH;
                case 'i': case 'I': return TILE_ENTITY_IMP;
                case 'e': case 'E': return TILE_ENTITY_IMP_OR_FILTH;
                default:
                    String str {};
                    str += F("Invalid TileEntity: ");
                    str += ch;
                    panic(PANIC_INVALID_ENUM, str.c_str());
                    return TILE_ENTITY_FIRE;
            }
        }

        bool isOptional(char ch) {
            return isLowerCase(ch);
        }

        game::Tile decodeTile(char ch) {
            Tile tile = Tile().what(decodeEntity(ch));
            if(isOptional(ch)) tile = tile.optional();
            return tile;
        }


        unsigned int size() const { return offsets.size(); }
        game::Pattern get(unsigned int index) const {
            Pattern pat;
            pat.tiles = tiles[*offsets[index]];

            unsigned int begin = *offsets[index];
            unsigned int end = (index + 1 < offsets.size()) ? *offsets[index + 1] : *offsets[offsets.size() - 1];

            pat.tileCount = end - begin; // Különbség ezen- és a következő offset közt = count.
            return pat;
        }

        void resizeTiles(unsigned int size) { tiles.resize(size); }
        unsigned int tilesSize() { return tiles.size(); }
        void resizeMarks(unsigned int size) { marks.resize(size); }
        unsigned int marksSize() { return marks.size(); }
        void resizeOffsets(unsigned int size) { offsets.resize(size); }
        unsigned int offsetsSize() { return offsets.size(); }

        unsigned int markCount() const { return marks.size(); }
        unsigned int getMark(unsigned int index) const {
            return *marks[index];
        }

        void printToSerial() {
            for(unsigned int i = 0; i < size(); i++) {
                Serial.print("#");
                Serial.print(i);
                Serial.println(": ");

                game::Pattern pat = get(i);
                for(byte j = 0; j < pat.tileCount; j++) {
                    Serial.println(pat.tiles[j].toString().c_str());
                }
            }
        }

    private:
        List<Tile> tiles {}; // Az összes tile egyben.
        List<byte> offsets {}; // Melyik pattern hol kezdődik.
        List<unsigned int> marks {}; // Melyik kategória hol kezdődik.

        static byte untilNextColumn(const char* top_p, const char* bottom_p) {
            byte count = 0;
            do {
                count++;
                top_p++; bottom_p++;
            } while(*top_p != ' ' && *top_p != '\0' && *bottom_p != ' ' && *bottom_p != '\0');
            return count;
        }

        /*static byte countNonSpaces(char* ptr) {
            byte count = 0;
            while(*ptr++ != '\0') {
                if(*ptr != ' ') count++;
            }
            return count;
        }*/
    };


    void createDefaultPatterns(PatternArray* patterns) {
        // TODO több pattern?
        // Itt van minden, hogy a string literálok hátha nem szennyezik a RAM-ot statikus változóként.
        patterns->resizeTiles(158);
        patterns->resizeOffsets(33);
        patterns->resizeMarks(9);

        // W: tűz
        // X: teljes fal
        // Y: repedt fal
        // Z: repedt/teljes fal (véletlen)
        // V: repedt fal/tűz (véletlen)
        // F: filth
        // I: imp
        // E: imp/filth (véletlen)
        // kisbetű: opcionális
        patterns->add("     ",
                      "ZWWWZ");

        patterns->add("y",
                      "z");
        patterns->add("z",
                      "y");
        patterns->add("y",
                      "y");

        patterns->add(" ",
                      "F");

        patterns->add("I",
                      " ");

        patterns->mark(); // 0. (Limbo)

        patterns->add("I",
                      " ");
        patterns->add(" ",
                      "E");

        patterns->add(" x ",
                      "Eee");

        patterns->add("E",
                      "Z");

        patterns->add("fff",
                      "zzz");

        patterns->mark(); // 1. (Lust)

        patterns->add("ZZZ",
                      "ZZZ");
        patterns->add("ff",
                      "ff");

        patterns->mark(); // 2. (Gluttony)

        patterns->add("      IY",
                      "ZWWWZ   ");
        patterns->add("I",
                      "W");

        patterns->mark(); // 3. (Greed)

        patterns->add(" zez ",
                      "zZFZz");
        patterns->add(" Z ",
                      "ZVZ");
        patterns->add(" X ",
                      "XXX");

        patterns->mark(); // 4. (Wrath)

        patterns->add("   w   ",
                      "w  y  w");
        patterns->add("w  y  w",
                      "   w   ");
        patterns->add("yzy",
                      "www");
        patterns->add(" e ",
                      "WWW");
        patterns->add("     ",
                      "XZVZX");
        patterns->add("XZZZX",
                      "     ");
        patterns->add("     ZXXx",
                      "xXXZ     ");
        patterns->add("xXXZ     ",
                      "     ZXXx");

        patterns->mark(); // 5. (Heresy)

        patterns->add("Z  f  Z",
                      "ZWfffWZ");
        patterns->add("  iZ",
                      "  iZ");

        patterns->mark(); // 6. (Violence)

        patterns->add(" fff ",
                      "fFEFf");
        patterns->add("X",
                      "X");

        patterns->mark(); // 7. (Fraud)

        patterns->add("yyzZzyy",
                      "Ww w wW");
        patterns->add("yXXy",
                      "yXXy");
        patterns->add("XeEe",
                      "XeEe");

        patterns->mark(); // 8. (Treachery)
        DEBUG_LOG_CAPTIONED(F("Pattern tiles: "), patterns->tilesSize());
        DEBUG_LOG_CAPTIONED(F("Pattern offsets: "), patterns->offsetsSize());
        DEBUG_LOG_CAPTIONED(F("Pattern marks: "), patterns->marksSize());
    };
    PatternArray defaultPatterns;


    class Layer {

    public:
        Layer(unsigned int length, byte framesPerStep, const char* name, byte minPause, byte maxPause) {
            this->length_m = length;
            this->framesPerStep_m = framesPerStep;
            this->name_m = name;
            this->minPause_m = minPause;
            this->maxPause_m = maxPause;
        }


        unsigned int length() const { return length_m; }
        byte framesPerStep() const { return framesPerStep_m; }
        const char* name() const { return name_m; }
        byte minPause() const { return minPause_m; }
        byte maxPause() const { return maxPause_m; }

    private:
        unsigned int length_m;
        byte framesPerStep_m;
        byte minPause_m;
        byte maxPause_m;
        const char* name_m;

    };

    const int LAYER_COUNT = 9;
    const Layer LAYERS[LAYER_COUNT] {
        Layer(90, 12, "LIMBO", 6, 10),
        Layer(100, 10, "LUST", 3, 8),
        Layer(250, 8, "GLUTTONY", 2, 7),
        Layer(400, 7, "GREED", 2, 6),
        Layer(600, 6, "WRATH", 2, 6),
        Layer(1000, 5, "HERESY", 1, 5),
        Layer(1500, 4, "VIOLENCE", 1, 4),
        Layer(2149, 3, "FRAUD", 1, 3),
        Layer(0xffff, 2, "TREACHERY", 0, 3)
    }; // A leghosszabb név 9 betű. A frames per step legyen páros, hogy a shotok tudjanak a felénél lépni.


    enum BlockmapFlags : byte {
        BLOCKMAP_FILTH = 1,
        BLOCKMAP_IMP = 2,

        BLOCKMAP_NOTHING = 0,
        BLOCKMAP_ENEMY = BLOCKMAP_FILTH | BLOCKMAP_IMP,
        BLOCKMAP_ALL = 4 - 1
    };


    class Scroller {
    public:
        const static byte WIDTH = LCD_WIDTH;
        const static byte HEIGHT = LCD_HEIGHT;

        bool indexValid(byte x, byte y) const {
            return x >= 0 && y >= 0 && x < WIDTH && y < HEIGHT;
        }

        byte* index(byte x, byte y) {
#if CONF_PANIC_BOUNDS
            if(!indexValid(x, y)) panic(PANIC_SCROLLER_INDEX_OUT_OF_RANGE);
#endif
            return &buffer[(y * WIDTH) + ((x + scroll) % WIDTH)];
        }

        void shiftLeft() {
            scroll++;
            scroll %= WIDTH;
        }

        void shiftLeft(byte fillWith) {
            for(byte y = 0; y < HEIGHT; y++) *index(0, y) = fillWith;
            shiftLeft();
        }

    private:
        byte buffer[WIDTH * HEIGHT] {};
        byte scroll = 0;
    };


    struct Stats {
        unsigned int steps = 0;
        unsigned int shots = 0;
        unsigned int parries = 0;
        unsigned int damageTaken = 0;
        unsigned int healingTaken = 0;

        void printJson() {
            Serial.print(F("{\"steps\":"));
            Serial.print(steps);
            Serial.print(F(",\"shots\":"));
            Serial.print(shots);
            Serial.print(F(",\"parries\":"));
            Serial.print(parries);
            Serial.print(F(",\"damageTaken\":"));
            Serial.print(damageTaken);
            Serial.print(F(",\"healingTaken\":"));
            Serial.print(healingTaken);
            Serial.print(F("}"));
        }
    };


    class SkullScene : public Scene {

    public:

        SkullScene(Stats stats) : stats(stats) {
            // do nothing
        }

        void setInputs(Buttons held, Buttons pressed, Buttons released) {
            buttonsHeld = held;
            buttonsPressed = pressed;
            buttonsReleased = released;
        }

        void process() {
            animationTimer++;
            animationTimer %= ANIMATION_PERIOD;

            if(buttonsReleased.right) {
                switchToMenu();
                return;
            }

            if(buttonsReleased.down && scroll < MAX_SCROLL) {
                scroll++;
            }
            if(buttonsReleased.up && scroll > 0) {
                scroll--;
            }
        }

        void draw(::LiquidCrystal* lcd_p) {
            setChars(lcd_p);

            gfx::Frame frame {};
            frame.clear(' ');

            for(byte y = 0; y < LCD_HEIGHT; y++) {
                drawRow(&frame, y, scroll + y);
            }

            frame.present(lcd_p);
        }

        void suspend() { /* do nothing */ }

        void resume(::LiquidCrystal* lcd_p) { /* do nothing */ };

    private:
        const Stats stats;
        Buttons buttonsHeld = {};
        Buttons buttonsPressed = {};
        Buttons buttonsReleased = {};
        byte animationTimer = 0;

        const byte MAX_SCROLL = 2 + 5 - LCD_HEIGHT; // 2 skull rows, 5 stats
        byte scroll = 0;

        const static byte CHAR_CROSSBONE_TL = 0;
        const static byte CHAR_CROSSBONE_TR = 1;
        const static byte CHAR_CROSSBONE_BL = 2;
        const static byte CHAR_CROSSBONE_BR = 3;
        const static byte CHAR_SKULL_TL = 4;
        const static byte CHAR_SKULL_TR = 5;
        const static byte CHAR_SKULL_BL = 6;
        const static byte CHAR_SKULL_BR = 7;

        const static byte ANIMATION_INTERVAL = 20;
        const static byte ANIMATION_PERIOD = 4 * ANIMATION_INTERVAL;

        void setChars(::LiquidCrystal* lcd_p) {
            byte flipped[8];

            // Csont
            lcd_p->createChar(CHAR_CROSSBONE_TL, (byte*)::gfx::sprites::CROSSBONE_TOP);

            memcpy(flipped, ::gfx::sprites::CROSSBONE_TOP, sizeof(flipped));
            ::gfx::sprites::flip(flipped);

            lcd_p->createChar(CHAR_CROSSBONE_TR, flipped);

            lcd_p->createChar(CHAR_CROSSBONE_BL, (byte*)::gfx::sprites::CROSSBONE_BOTTOM);

            memcpy(flipped, ::gfx::sprites::CROSSBONE_BOTTOM, sizeof(flipped));
            ::gfx::sprites::flip(flipped);
            lcd_p->createChar(CHAR_CROSSBONE_BR, flipped);

            // 💀
            const byte* top;
            const byte* bottom;
            if(animationTimer % (ANIMATION_INTERVAL * 2) < ANIMATION_INTERVAL) {
                top = ::gfx::sprites::SKULL1_TOP;
                bottom = ::gfx::sprites::SKULL1_BOTTOM;
            } else {
                top = ::gfx::sprites::SKULL2_TOP;
                bottom = ::gfx::sprites::SKULL2_BOTTOM;
            }

            lcd_p->createChar(CHAR_SKULL_TL, (byte*)top);

            memcpy(flipped, top, sizeof(flipped));
            ::gfx::sprites::flip(flipped);

            lcd_p->createChar(CHAR_SKULL_TR, flipped);

            lcd_p->createChar(CHAR_SKULL_BL, (byte*)bottom);

            memcpy(flipped, bottom, sizeof(flipped));
            ::gfx::sprites::flip(flipped);
            lcd_p->createChar(CHAR_SKULL_BR, flipped);
        }

        void drawRow(::gfx::Frame* frame_p, byte y, byte row) {
            byte mid = LCD_WIDTH / 2 - 1;

            // Legrosszabb megoldás ever! (pusztító switch)
            switch(row) {
                case 0:
                    *frame_p->index(mid - 1, y) = CHAR_CROSSBONE_TL;
                    *frame_p->index(mid + 2, y) = CHAR_CROSSBONE_TR;
                    *frame_p->index(mid - 0, y) = CHAR_SKULL_TL;
                    *frame_p->index(mid + 1, y) = CHAR_SKULL_TR;

                    frame_p->write(mid - 6, y, "Game");
                    frame_p->write(mid + 4, y, "over");
                    break;

                case 1:
                    *frame_p->index(mid - 1, y) = CHAR_CROSSBONE_BL;
                    *frame_p->index(mid + 2, y) = CHAR_CROSSBONE_BR;

                    *frame_p->index(mid - 0, y) = CHAR_SKULL_BL;
                    *frame_p->index(mid + 1, y) = CHAR_SKULL_BR;

                    byte haX;
                    if(animationTimer % (ANIMATION_INTERVAL * 2) >= ANIMATION_INTERVAL) {
                        if(animationTimer < ANIMATION_INTERVAL * 2) {
                            haX = mid - 5;
                        } else {
                            haX = mid + 5;
                        }
                        frame_p->write(haX, y, "HA");
                    }
                    break;

                case 2:
                    frame_p->write(0, y, "steps");
                    frame_p->writeRtl(LCD_WIDTH - 1, y, stats.steps);
                    break;
                case 3:
                    frame_p->write(0, y, "shots");
                    frame_p->writeRtl(LCD_WIDTH - 1, y, stats.shots);
                    break;
                case 4:
                    frame_p->write(0, y, "parries");
                    frame_p->writeRtl(LCD_WIDTH - 1, y, stats.parries);
                    break;
                case 5:
                    frame_p->write(0, y, "damage");
                    frame_p->writeRtl(LCD_WIDTH - 1, y, stats.damageTaken);
                    break;
                case 6:
                    frame_p->write(0, y, "healing");
                    frame_p->writeRtl(LCD_WIDTH - 1, y, stats.healingTaken);
                    break;


            }
        }

    };


    class Game : public Scene {

    public:

        Game(uint16_t seed) : patterns(&defaultPatterns), levelSeed(seed) {
            bufferB.clear();

            switchLayer(0);
            patternPause = 0;
        }


        void setInputs(::Buttons held, ::Buttons pressed, ::Buttons released) {
            buttonsHeld = held;
            buttonsPressed = pressed;
            buttonsReleased = released;
        }

        void suspend() { /* do nothing */ }

        void resume(::LiquidCrystal* lcd_p) {
            initializeCustomChars(lcd_p);
        }

        void process() {
            if(dieTimer > 0) {
                dieTimer++;
                if(dieTimer > 30) {
                    Serial.print(F("[Stats] "));
                    stats.printJson();
                    Serial.println();
                    switchToSkull(stats);
                }
                return;
            }

            processPlayer();

            if(parryFlash > 0) {
                parryFlash--;
                return;
            }

            // Animációk
            processTimer++;
            fire.advancePhase();

            // Blockmap (OPTIMIZE: nem minden processkor updatel, csak ha kell)
            updateBlockmap();

            // Különböző mapek fogyása
            if(processTimer % 2 == 0) {
                for(byte y = 0; y < bloodMap.HEIGHT; y++) {
                    for(byte x = 0; x < bloodMap.WIDTH; x++) {
                        byte* blood = bloodMap.index(x, y);
                        if(*blood > 0) {
                            byte loss = max(1, *blood / 8);
                            *blood -= loss;
                        }

                        if(y + 1 < bloodMap.HEIGHT && (*obstacleMap.index(x, y + 1) == OBSTACLE_EMPTY || *obstacleMap.index(x, y + 1) == OBSTACLE_FIRE)) {
                            byte* under = bloodMap.index(x, y + 1);
                            byte transferable = min(byte(255 - *under), *blood);
                            *blood -= transferable;
                            *under += transferable;
                        }
                    }
                }
            } else {
                for(byte y = 0; y < smokeMap.HEIGHT; y++) {
                    for(byte x = 0; x < smokeMap.WIDTH; x++) {
                        byte* smoke = smokeMap.index(x, y);
                        if(*smoke > 0) (*smoke)--;
                    }
                }
            }

            stepTimer++;
            if(stepTimer >= layer_p->framesPerStep()) {
                stepTimer = 0;
                stepAllway();
            } else if(stepTimer == (layer_p->framesPerStep() / 2)) {
                stepHalfway();
            }
        }

        void draw(::LiquidCrystal* lcd_p) {
            if(dieTimer > 0) {
                *frame_p->index(effectRandom.nextUint8(LCD_WIDTH), effectRandom.nextUint8(LCD_HEIGHT)) = ::gfx::HEX_DIGITS[effectRandom.nextUint8(16)];

                if(dieTimer % 4 == 0) {
                    byte x = effectRandom.nextUint8(LCD_WIDTH);
                    byte y = effectRandom.nextUint8(LCD_HEIGHT);
                    byte i = effectRandom.nextUint8(::gfx::DIE_PHRASE_COUNT);
                    frame_p->write(x, y, ::gfx::DIE_PHRASES[i]);
                }
            } else {
                frame_p->clear();

                // Entitások
                if(frameNumber % 2 == 0) { // Genuis!
                    drawObstacles(frame_p);
                    drawSmoke(frame_p);
                    drawBlood(frame_p);
                    drawShots(frame_p);
                } else {
                    drawShots(frame_p);
                    drawObstacles(frame_p);
                    drawSmoke(frame_p);
                    drawBlood(frame_p);
                }

                drawPlayer(frame_p);
                drawFilths(frame_p);
                drawImps(frame_p);

                if(parryFlash > 0 && frameNumber % 2 != 0) {
                    overlayFrame = ::gfx::sprites::FLASH_FRAMES - (parryFlash * PARRY_FLASH_LENGTH / ::gfx::sprites::FLASH_FRAMES);
                    drawOverlay(frame_p);
                }

                drawHud(frame_p);
            }

            // Prezentálás
            setCustomChars(lcd_p);
            frame_p->presentDifferential(lcd_p, backPtr);

            // Bufferek felcsereberélése
            gfx::Frame* tmp = frame_p;
            frame_p = backPtr;
            backPtr = tmp;

            frameNumber++;
            effectRandom.warmup(1);
        }


        void initializeCustomChars(::LiquidCrystal* lcd_p) {
            // A const sajnos elcastolandó, mert a LiquidCrystal hülye.
            lcd_p->createChar(gfx::CHAR_WALL, (byte*)gfx::sprites::WALL);
            lcd_p->createChar(gfx::CHAR_CRACKED_WALL, (byte*)gfx::sprites::CRACKED_WALL);

            actualPlayerFrame = 255;
            actualFilthFrame = 255;
            actualImpFrame = 255;
            actualOverlayFrame = 255;
            setCustomChars(lcd_p);
        }


        void bannerClear() {
            memset(&banner, 0, BANNER_SIZE * sizeof(byte));
            bannerLength = 0;
        }

        void bannerAppend(const char* text) {
            while(*text != '\0' && bannerLength < BANNER_SIZE) banner[bannerLength++] = *text++;
        }

        void bannerAppend(StringWrapper* text) {
            unsigned int i = 0;
            while((*text)[i] != '\0' && bannerLength < BANNER_SIZE) banner[bannerLength++] = (*text)[i++];
        }

        void bannerAppend(byte n) {
            // TODO multi-digit number support
            if(bannerLength < BANNER_SIZE) banner[bannerLength++] = '0' + n;
        }

        void bannerStart() {
            bannerScroll = 0;
        }

    private:
        gfx::Frame bufferA;
        gfx::Frame bufferB;
        gfx::Frame* frame_p = &bufferA;
        gfx::Frame* backPtr = &bufferB;

        Stats stats {};

        static const byte BANNER_SIZE = 20;
        char banner[BANNER_SIZE];
        byte bannerLength = 0;
        unsigned int bannerScroll = 0;

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

        Buttons buttonsHeld = {};
        Buttons buttonsPressed = {};
        Buttons buttonsReleased = {};

        gfx::Fire fire {};

        byte processTimer = 0; // processenként nő
        byte bloodDrainTimer = 0; // stepHalfwayenként nő

        byte playerFrame = 0;
        byte actualPlayerFrame = 255;
        byte filthFrame = 0;
        byte actualFilthFrame = 255;
        byte impFrame = 0;
        byte actualImpFrame = 255;
        byte overlayFrame = 0;
        byte actualOverlayFrame = 255;

        byte flashDuration = 0;
        byte flashProgress = 0;
        byte frameNumber = 0; // drawenként nő


        const game::PatternArray* const patterns;
        const uint16_t levelSeed;
        ::Random levelRandom { 0 };
        ::Random effectRandom { 1 };
        const game::Layer* layer_p;
        byte layerNumber = 0;
        unsigned int layerStepsLeft = 0;

        ::game::Pattern pattern { 0, NULL };
        byte patternProgress = 0;
        byte patternPause = 0;
        static const byte LAYER_INTERMISSION = LCD_WIDTH / 4;

        byte stepTimer = 0;


        byte parryFlash = 0;
        byte parryCooldown = 0;
        static const byte PARRY_FLASH_LENGTH = 10;
        static const byte PARRY_COOLDOWN = 10;
        byte shootCooldown = 0;
        static const byte SHOOT_COOLDOWN = 10;
        byte shootCharge = 0;
        static const byte SHOOT_CHARGE_STEP_LENGTH = 4;
        bool playerUp = false;
        byte playerIFrames = 0;
        byte leapTime = 0;

        static const byte MAX_HEALTH = CHAR_WIDTH * CHAR_HEIGHT;
        byte playerHealth = MAX_HEALTH;
        byte playerHealthFlash = 0;
        byte dieTimer = 0;

        static const byte DAMAGE_MELEE = 10;
        static const byte DAMAGE_WALL = 4;
        static const byte DAMAGE_SHOT = 8;
        static const byte DAMAGE_FIRE = 2;


        BlockmapFlags blockmap[LCD_WIDTH * LCD_HEIGHT];
        Vec<Shot, 24> shots {};
        Scroller obstacleMap {};
        Scroller smokeMap {};
        Scroller bloodMap {};
        Vec<Filth, 16> filths {};
        Vec<Imp, 16> imps {};
        byte impTimer = 0;


        void setCustomChars(::LiquidCrystal* lcd_p) {
            if(actualPlayerFrame != playerFrame) {
                const byte* sprite;
                switch(playerFrame) {
                    default:
                    case 0: sprite = ::gfx::sprites::PLAYER_WALK1; break;
                    case 1: sprite = ::gfx::sprites::PLAYER_WALK2; break;
                    case 2: sprite = ::gfx::sprites::PLAYER_SLIDE; break;
                }
                lcd_p->createChar(::gfx::CHAR_PLAYER, (byte*)sprite);
                actualPlayerFrame = playerFrame;
            }
            if(actualFilthFrame != filthFrame) {
                const byte* sprite;
                switch(filthFrame) {
                    default:
                    case 0: sprite = ::gfx::sprites::FILTH1; break;
                    case 1: sprite = ::gfx::sprites::FILTH2; break;
                }
                lcd_p->createChar(::gfx::CHAR_FILTH, (byte*)sprite);
                actualFilthFrame = filthFrame;
            }
            if(actualImpFrame != impFrame) {
                const byte* sprite;
                switch(impFrame) {
                    default:
                    case 0: sprite = ::gfx::sprites::IMP1; break;
                    case 1: sprite = ::gfx::sprites::IMP2; break;
                    case 2: sprite = ::gfx::sprites::IMP_FIRE; break;
                }
                lcd_p->createChar(::gfx::CHAR_IMP, (byte*)sprite);
                actualImpFrame = impFrame;
            }
            if(actualOverlayFrame != overlayFrame) {
                const byte* sprite;
                switch(overlayFrame) {
                    default:
                    case 0: sprite = ::gfx::sprites::FLASH0; break;
                    case 1: sprite = ::gfx::sprites::FLASH1; break;
                    case 2: sprite = ::gfx::sprites::FLASH2; break;
                    case 3: sprite = ::gfx::sprites::FLASH3; break;
                }
                lcd_p->createChar(::gfx::CHAR_OVERLAY, (byte*)sprite);
                actualOverlayFrame = overlayFrame;
            }

            // Tűzkarakter
            byte composedFire[CHAR_HEIGHT];
            fire.compose(composedFire);
            lcd_p->createChar(gfx::CHAR_FIRE, composedFire);

            // Életkarakter
            if(playerHealthFlash == 0) {
                byte juice[CHAR_HEIGHT] {};
                byte bitsLeft = playerHealth;
                for(byte y = 0; y < CHAR_HEIGHT && bitsLeft > 0; y++) {
                    for(byte x = 0; x < CHAR_WIDTH && bitsLeft > 0; x++) {
                        byte shift = (x + frameNumber) % CHAR_WIDTH;
                        juice[CHAR_HEIGHT - 1 - y] |= 1 << (CHAR_WIDTH - shift - 1);
                        bitsLeft--;
                    }
                }
                lcd_p->createChar(gfx::CHAR_HEALTH, juice);
            }
        }


        void switchLayer(byte num) {
            //DEBUG_LOG_CAPTIONED(F("Layer "), num + 1);
            layerNumber = num;
            layer_p = &LAYERS[num];
            layerStepsLeft = layer_p->length();

            auto text1 = FlashStringWrapper(F("LAYER "));
            bannerAppend(&text1);
            bannerAppend(num + 1);
            auto text2 = FlashStringWrapper(F(": "));
            bannerAppend(&text2);
            bannerAppend(layer_p->name());
            bannerStart();

            pattern = Pattern { 0, NULL };
            patternProgress = 0;
            patternPause += LAYER_INTERMISSION;

            levelRandom.reseed(levelSeed + layerNumber * 149);
            levelRandom.warmup(20);
        }

        game::Pattern getRandomPattern() {
            unsigned int limit = patterns->getMark(min((unsigned int)layerNumber, patterns->markCount() - 1));
            unsigned int index = levelRandom.nextUint16(limit);
            return patterns->get(index);
        }

        void advancePattern() {
            if(patternPause > 0) patternPause--;
            while(true) {
                while(pattern.tiles == NULL || patternProgress >= pattern.tileCount) {
                    pattern = getRandomPattern();

                    patternProgress = 0;

                    byte pause = layer_p->minPause() + levelRandom.nextUint8(layer_p->maxPause() - layer_p->minPause());
                    patternPause += pause;
                }
                if(patternPause > 0) break;

                Tile tile = pattern.tiles[patternProgress++];
                patternPause += tile.getMoves();

                if(tile.isOptional() && levelRandom.nextBit()) continue;

                byte x = LCD_WIDTH - 1;
                byte y = tile.isTop() ? 0 : 1;
                TileEntity what = tile.getWhat();
                sw: switch(what) {
                    case TILE_ENTITY_FIRE:
                        *obstacleMap.index(x, y) = OBSTACLE_FIRE;
                        break;

                    case TILE_ENTITY_CRACKED_WALL:
                        *obstacleMap.index(x, y) = OBSTACLE_CRACKED_WALL;
                        break;

                    case TILE_ENTITY_WALL:
                        *obstacleMap.index(x, y) = OBSTACLE_WALL;
                        break;

                    case TILE_ENTITY_CRACKED_WALL_OR_WALL:
                        what = levelRandom.nextBit() ? TILE_ENTITY_CRACKED_WALL : TILE_ENTITY_WALL;
                        goto sw;

                    case TILE_ENTITY_CRACKED_WALL_OR_FIRE:
                        what = levelRandom.nextBit() ? TILE_ENTITY_CRACKED_WALL : TILE_ENTITY_FIRE;
                        goto sw;

                    case TILE_ENTITY_FILTH:
                        if(!filths.tryPush(Filth(x, y))) {
#if CONF_DEBUG_LOG_PUSH_FAILURES
                            DEBUG_LOG(F("tryPush Filth failed"));
#endif
                        }
                        break;

                    case TILE_ENTITY_IMP:
                        if(!imps.tryPush(Imp(x, y))) {
#if CONF_DEBUG_LOG_PUSH_FAILURES
                            DEBUG_LOG(F("tryPush Imp failed"));
#endif
                        }
                        break;

                    case TILE_ENTITY_IMP_OR_FILTH:
                        what = levelRandom.nextBit() ? TILE_ENTITY_IMP : TILE_ENTITY_FILTH;
                        goto sw;
                }
            }
        }


        void stepHalfway() {
            stepShots();
            dealShotDamages();
            stepFilths();
            stepImps();

            updateBlockmap();

            dealShotDamages();

            if(leapTime > 0) stepAllway(); // Genuis! (enemies aren't updated)

            // Vércsökkentés
            bloodDrainTimer++;
            if(playerHealth > 1) {
                if(playerHealth > MAX_HEALTH) playerHealth = MAX_HEALTH;

                byte drainOnceEvery;
                if(playerHealth > 20) drainOnceEvery = 1;
                else if(playerHealth > 15) drainOnceEvery = 2;
                else if(playerHealth > 10) drainOnceEvery = 4;
                else if(playerHealth > 5) drainOnceEvery = 8;
                else drainOnceEvery = 16;

                if((bloodDrainTimer % drainOnceEvery) == 0) playerHealth--;
            }
        }

        void stepAllway() {
            stepLevel();

            dealShotDamages();

            stepShots();
            dealShotDamages();
        }


        void updateBlockmap() {
            memset(blockmap, 0, sizeof(blockmap));

            for(byte i = 0; i < filths.size(); i++) {
                Entity* ent = filths[i];
                safeDisjunctBlockmap(ent->posX, ent->posY, BLOCKMAP_FILTH);
            }

            for(byte i = 0; i < imps.size(); i++) {
                Entity* ent = imps[i];
                safeDisjunctBlockmap(ent->posX, ent->posY, BLOCKMAP_IMP);
            }
        }


        bool blockmapIndexValid(byte x, byte y) {
            return x >= 0 && y >= 0 && x < LCD_WIDTH && y < LCD_HEIGHT;
        }

        ::game::BlockmapFlags* indexBlockmap(byte x, byte y) {
#if CONF_PANIC_BOUNDS
            if(!blockmapIndexValid(x, y)) panic(PANIC_BLOCKMAP_INDEX_OUT_OF_RANGE);
#endif
            return &blockmap[(y * LCD_WIDTH) + x];
        }

        ::game::BlockmapFlags getBlockmap(byte x, byte y) { return *indexBlockmap(x, y); }

        ::game::BlockmapFlags safeGetBlockmap(byte x, byte y) {
            if(blockmapIndexValid(x, y)) return blockmap[(y * LCD_WIDTH) + x];
            else return BLOCKMAP_NOTHING;
        }

        void safeDisjunctBlockmap(byte x, byte y, ::game::BlockmapFlags what) {
            if(!blockmapIndexValid(x, y)) return;
            blockmap[(y * LCD_WIDTH) + x] = (BlockmapFlags)(blockmap[(y * LCD_WIDTH) + x] | what);
        }

        void safeConjunctBlockmap(byte x, byte y, ::game::BlockmapFlags what) {
            if(!blockmapIndexValid(x, y)) return;
            blockmap[(y * LCD_WIDTH) + x] = (BlockmapFlags)(blockmap[(y * LCD_WIDTH) + x] & ~what);
        }


        void drawHud(::gfx::Frame* frame_p) {

            // Banner
            if((frameNumber % 3 == 0) && bannerLength > 0) {
                int posX = LCD_WIDTH - 1 - bannerScroll;
                unsigned int i = max(0, -posX);
                byte x = max(0, posX);
                while(x < LCD_WIDTH - 1 && i < bannerLength) {
                    *frame_p->index(x, 0) = banner[i];
                    i++;
                    x++;
                }

                if((frameNumber % 2) == 0) bannerScroll++;
                if(bannerScroll >= (byte)(bannerLength + LCD_WIDTH)) {
                    bannerClear();
                }
            }

            // Élet
            char topCh = ' ';
            if(playerHealthFlash > 0) {
                topCh = '!';
                playerHealthFlash--;
            } else {
                topCh = ::gfx::CHAR_HEALTH;
            }
            *frame_p->index(LCD_WIDTH - 1, 0) = topCh;
            // Réteg/chargeolás
            char bottomCh = ' ';
            if(shootCharge > SHOOT_CHARGE_STEP_LENGTH) {
                if(!shootFullyCharged() || frameNumber % 8 >= 4) {
                    bottomCh = ::gfx::SHOOT_CHARGE_CHARS_F[shootCharge / SHOOT_CHARGE_STEP_LENGTH];
                }
            } else {
                bottomCh = '0' + (layerNumber + 1);
            }
            *frame_p->index(LCD_WIDTH - 1, LCD_HEIGHT - 1) = bottomCh;
        }


        byte playerX() const { return 0; }
        byte playerY() const { return playerUp ? 0 : 1; }
        bool shootFullyCharged() const { return ::gfx::SHOOT_CHARGE_CHARS_F[shootCharge / SHOOT_CHARGE_STEP_LENGTH + 1] == '\0'; }

        void hitPlayer(byte unscaledDamage, bool grantIFrames) {
            if(playerIFrames > 0) return;
            if(grantIFrames) playerIFrames = 16;

            // TODO skálázás nehézséggel
            byte damage = unscaledDamage;

            if(playerHealth >= damage) {
                stats.damageTaken += damage;
                playerHealth -= damage;
            } else {
                stats.damageTaken += playerHealth;
                playerHealth = 0;
            }

            playerHealthFlash = 4;
            //showBanner(String("Ouch!"));
        }

        void hitPlayer(byte unscaledDamage) { hitPlayer(unscaledDamage, true); }

        void healPlayer(byte amount) {
            if(playerHealth + amount >= MAX_HEALTH) {
                stats.healingTaken += MAX_HEALTH - playerHealth;
                playerHealth = MAX_HEALTH;
            } else {
                stats.healingTaken += amount;
                playerHealth += amount;
            }
        }


        void processPlayer() {
            if(playerIFrames > 0) playerIFrames--;
            if(parryCooldown > 0) parryCooldown--;
            if(shootCooldown > 0) shootCooldown--;

            // TODO ground slam?
            // Ugrálás
            if(playerUp) {
                playerUp = buttonsHeld.up || (*obstacleMap.index(playerX(), 1) != OBSTACLE_EMPTY);
            } else {
                playerUp = buttonsHeld.up && (*obstacleMap.index(playerX(), 0) == OBSTACLE_EMPTY);
            }

            if(buttonsHeld.up) {
                playerFrame = 0;
                //hitPlayer(255);
            } else if(buttonsHeld.down) {
                playerFrame = 2;
                leapTime = 2;
            } else {
                playerFrame = (stepTimer >= layer_p->framesPerStep() / 2) ? 1 : 0;
            }

            if(!buttonsHeld.up && !buttonsHeld.down && leapTime > 0) leapTime--;


            // Parryzás
            if(buttonsPressed.right) {
                bool parried = false;
                if(parryCooldown == 0) {
                    for(byte i = 0; i < shots.size(); i++) {
                        Shot* ent = shots[i];
                        if(!ent->friendly() && ent->posY == playerY() && ent->posX <= 1) {
                            parryFlash = PARRY_FLASH_LENGTH;
                            ent->setFriendly(true);
                            ent->posX = 1;
                            ent->setExplosive(true);

                            if(!parried) {
                                playerIFrames = max(playerIFrames, (byte)5);
                                healPlayer(MAX_HEALTH / 2);
                            }
                            parried = true;
                            stats.parries++;
                        }
                    }
                }
                parryCooldown = PARRY_COOLDOWN;

                if(!parried && shootCharge == 0) shootCharge = 1;
            }

            // Lövés
            if(buttonsReleased.right && shootCooldown <= 0 && shootCharge > 0) {
                bool explosive = shootFullyCharged();
                if(shots.forcePush(Shot(playerX(), playerY(), true, explosive))) {
#if CONF_DEBUG_LOG_PUSH_FAILURES
                    DEBUG_LOG(F("Force pushed player shot"));
#endif
                }
                shootCharge = 0;
                shootCooldown = SHOOT_COOLDOWN;
                stats.shots++;
            }

            if(buttonsHeld.right && shootCharge > 0) {
                if(!shootFullyCharged()) shootCharge++;
            } else {
                shootCharge = 0;
            }

            // Vér ivás
            byte* blood_p = bloodMap.index(playerX(), playerY());
            if(*blood_p > 0) {
                healPlayer(*blood_p / 8);
                *blood_p = 0;
            }

            if(playerHealth <= 0 && dieTimer == 0) {
                dieTimer = 1;
                DEBUG_LOG(F("R.I.P."));
            }
        }

        void drawPlayer(::gfx::Frame* frame_p) {
            if(playerIFrames == 0 || (frameNumber % 8 < 4)) {
                *frame_p->index(playerX(), playerY()) = ::gfx::CHAR_PLAYER;
            }
        }


        void drawOverlay(::gfx::Frame* frame_p) {
            frame_p->clear(::gfx::CHAR_OVERLAY);
        }


        void drawShots(::gfx::Frame* frame_p) {
            for(byte i = 0; i < shots.size(); i++) {
                Shot* ent = shots[i];

                char sprite;
                if(ent->explosive()) sprite = '*';
                else sprite = ent->friendly() ? '-' : '+';

                *frame_p->index(ent->posX, ent->posY) = sprite;
            }
        }

        void stepShots() {
            byte i = shots.size();
            while(i > 0) {
                i--;
                Shot* ent = shots[i];

                if(ent->friendly()) {
                    if(ent->posX >= LCD_WIDTH - 1) {
                        shots.removeAt(i);
                        continue;
                    }

                    ent->posX++;
                } else {
                    if(ent->posX == playerX() && ent->posY == playerY()) {
                        hitPlayer(DAMAGE_SHOT);
                    }

                    if(ent->posX <= 0) {
                        shots.removeAt(i);
                        continue;
                    }
                    ent->posX--;
                }
            }
        }

        void dealShotDamages() {
            byte i = shots.size();
            while(i > 0) {
                i--;
                Shot* ent = shots[i];

                bool impacted = false;
                if(ent->explosive()) {
                    // Direkt nem sebzünk, mert a robbanás violentebb és több vért csinál.
                    if(
                        (obstacleMap.indexValid(ent->posX, ent->posY)
                         && *obstacleMap.index(ent->posX, ent->posY) != OBSTACLE_EMPTY
                         && *obstacleMap.index(ent->posX, ent->posY) != OBSTACLE_FIRE
                        ) || ((safeGetBlockmap(ent->posX, ent->posY) & BLOCKMAP_ENEMY) != 0)
                    ) {
                        explodeAt(ent->posX, ent->posY);
                        impacted = true;
                    }
                } else {
                    impacted = hitObstacleAt(ent->posX, ent->posY)
                        || (ent->friendly() && hitEnemyAt(ent->posX, ent->posY, 1));
                }

                if(impacted) {
                    shots.removeAt(i);
                    continue;
                }
            }
        }


        // Visszaadja a sebzések számát.
        byte hitObstacleAt(byte x, byte y, byte maxHits) {
            if(maxHits == 0 || !obstacleMap.indexValid(x, y)) return false;

            ObstacleKind* obst_p = (ObstacleKind*)obstacleMap.index(x, y);
            switch(*obst_p) {
                case OBSTACLE_WALL:
                    if(maxHits >= 2) {
                        *obst_p = OBSTACLE_EMPTY;
                        return 2;
                    } else {
                        *obst_p = OBSTACLE_CRACKED_WALL;
                        return 1;
                    }
                case OBSTACLE_CRACKED_WALL:
                    *obst_p = OBSTACLE_EMPTY;
                    return 1;

                default: return 0;
            }
        }

        bool hitObstacleAt(byte x, byte y) {
            return hitObstacleAt(x, y, 1) == 1;
        }


        byte addBloodAt(byte x, byte y, unsigned int amount) {
            byte* ptr = bloodMap.index(x, y);
            byte lack = 255 - *ptr;

            if(lack >= amount) {
                *ptr += amount;
                return amount;
            } else {
                *ptr = 255;
                return lack;
            }
        }

        void createBloodAt(byte x, byte y, unsigned int amount) {
            byte limit = max(x, byte((bloodMap.WIDTH - 1) - x));

            for(byte i = 0; i <= limit && amount > 0; i++) {
                amount -= addBloodAt(x - i, y, amount);
                amount -= addBloodAt(x + i, y, amount);
            }
        }


        // Visszaadja a sebzések számát.
        bool hitEnemyAt(byte x, byte y, byte violence, byte maxHits) {
            if((safeGetBlockmap(x, y) & BLOCKMAP_ENEMY) == 0) return false;

            for(byte i = 0; i < filths.size(); i++) {
                Filth* ent = filths[i];
                if(ent->posX != x || ent->posY != y) continue;

                createBloodAt(ent->posX, ent->posY, 96 * violence);
                filths.removeAt(i);

                return 1;
            }

            for(byte i = 0; i < imps.size(); i++) {
                Imp* ent = imps[i];
                if(ent->posX != x || ent->posY != y) continue;

                if(ent->health <= maxHits) {
                    createBloodAt(ent->posX, ent->posY, 128 * violence);
                    imps.removeAt(i);
                    return ent->health;
                } else {
                    ent->health -= maxHits;
                    return maxHits;
                }
            }

            return 0;
        }

        bool hitEnemyAt(byte x, byte y, byte violence) {
            return hitEnemyAt(x, y, violence, 1) == 1;
        }


        // Visszaadja a sebzések számát.
        byte explodeAt(byte x, byte y) {
            byte total = 0;

            byte xMin = (x > 0) ? x - 1 : x;
            byte xMax = (x < LCD_WIDTH - 1) ? x + 1 : x;
            byte yMin = (y > 0) ? y - 1 : y;
            byte yMax = (y < LCD_HEIGHT - 1) ? y + 1 : y;

            for(byte j = yMin; j <= yMax; j++) {
                for(byte i = xMin; i <= xMax; i++) {
                    byte dist = (i < x ? x - i : i - x) + (j < y ? y - j : j - y); // Unsignedítisz
                    byte damage = 3 - dist;

                    total += hitObstacleAt(i, j, damage);
                    total += hitEnemyAt(i, j, damage, damage);

                    byte* smoke_p = smokeMap.index(i, j);
                    *smoke_p = min(byte(*smoke_p + (::gfx::SMOKE_MAX - dist)), ::gfx::SMOKE_MAX);
                }
            }

            return total;
        }


        void drawObstacles(::gfx::Frame* frame_p) {
            for(byte y = 0; y < LCD_HEIGHT; y++) {
                for(byte x = 0; x < LCD_WIDTH; x++) {
                    char ch;
                    switch(*obstacleMap.index(x, y)) {
                        case OBSTACLE_EMPTY: ch = ' '; break;
                        case OBSTACLE_CRACKED_WALL: ch = ::gfx::CHAR_CRACKED_WALL; break;
                        case OBSTACLE_WALL: ch = ::gfx::CHAR_WALL; break;
                        case OBSTACLE_FIRE: ch = ::gfx::CHAR_FIRE; break;
                        default: panic(PANIC_INVALID_ENUM);
                    }
                    if(ch != ' ') *frame_p->index(x, y) = ch;
                }
            }
        }

        void drawBlood(::gfx::Frame* frame_p) {
            for(byte y = 0; y < LCD_HEIGHT; y++) {
                for(byte x = 0; x < LCD_WIDTH; x++) {
                    byte blood = *bloodMap.index(x, y);

                    char ch;
                    byte flickerPhase = frameNumber / 2;

                    if(blood >= 128) ch = ::gfx::BLOOD;
                    else if(blood >= 64 && (flickerPhase % 2 == 0)) ch = ::gfx::BLOOD;
                    else if(blood >= 32 && (flickerPhase % 3 == 0)) ch = ::gfx::BLOOD;
                    else if(blood > 0 && (flickerPhase % 4 == 0)) ch = ::gfx::BLOOD;
                    else ch = '\0';

                    if(ch != '\0') *frame_p->index(x, y) = ch;
                }
            }
        }

        void drawSmoke(::gfx::Frame* frame_p) {
            for(byte y = 0; y < LCD_HEIGHT; y++) {
                for(byte x = 0; x < LCD_WIDTH; x++) {
                    byte smoke = *smokeMap.index(x, y);
                    if(smoke > 0) {
                        *frame_p->index(x, y) = ::gfx::SMOKE[min(smoke, ::gfx::SMOKE_MAX)];;
                    }
                }
            }
        }

        void stepLevel() {
            obstacleMap.shiftLeft(OBSTACLE_EMPTY);
            bloodMap.shiftLeft(0);
            smokeMap.shiftLeft(0);

            byte i = filths.size();
            while(i > 0) {
                i--;
                Filth* ent = filths[i];

                if(ent->posX == 0) filths.removeAt(i);
                else ent->posX--;
            }

            i = imps.size();
            while(i > 0) {
                i--;
                Imp* ent = imps[i];

                if(ent->posX == 0) imps.removeAt(i);
                else ent->posX--;
            }

            advancePattern();


            updateBlockmap();
            BlockmapFlags bm = getBlockmap(playerX(), playerY());
            ObstacleKind* obst_p = (ObstacleKind*)obstacleMap.index(playerX(), playerY());
            if((bm & BLOCKMAP_ENEMY) > 0) {
                hitPlayer(DAMAGE_MELEE);
            } else if(*obst_p == OBSTACLE_WALL || *obst_p == OBSTACLE_CRACKED_WALL) {
                hitPlayer(DAMAGE_WALL, false);
                *obst_p = OBSTACLE_EMPTY;
            } else if(*obst_p == OBSTACLE_FIRE) {
                hitPlayer(DAMAGE_FIRE, false);
            }

            if(layerStepsLeft <= 0 && layerNumber < game::LAYER_COUNT - 1) {
                switchLayer(layerNumber + 1);
            } else {
                layerStepsLeft--;
            }

            stats.steps++;
        }


        void drawFilths(::gfx::Frame* frame_p) {
            for(byte i = 0; i < filths.size(); i++) {
                Filth* ent = filths[i];
                *frame_p->index(ent->posX, ent->posY) = ::gfx::CHAR_FILTH;
            }
        }

        void stepFilths() {
            filthFrame = (filthFrame + 1) % 2;
            for(byte i = 0; i < filths.size(); i++) {
                Filth* ent = filths[i];

                if(ent->posY >= 1) continue;
                if(*obstacleMap.index(ent->posX, ent->posY + 1) != OBSTACLE_EMPTY) continue;
                if(getBlockmap(ent->posX, ent->posY + 1) != BLOCKMAP_NOTHING) continue;

                ent->posY++;
            }
        }


        void drawImps(::gfx::Frame* frame_p) {
            for(byte i = 0; i < imps.size(); i++) {
                Imp* ent = imps[i];
                *frame_p->index(ent->posX, ent->posY) = ::gfx::CHAR_IMP;
            }
        }

        void stepImps() {
            impTimer++;
            impTimer %= 10;

            if(impTimer > 7) {
                impFrame = 2;
            } else {
                impFrame = (impTimer / 2) % 2;
            }

            if(impTimer <= 8 && impTimer % 2 == 0) {
                for(byte i = 0; i < imps.size(); i++) {
                    Imp* ent = imps[i];

                    if(ent->posX >= LCD_WIDTH - 1) continue;
                    if(*obstacleMap.index(ent->posX + 1, ent->posY) != OBSTACLE_EMPTY) continue;
                    if(getBlockmap(ent->posX + 1, ent->posY) != BLOCKMAP_NOTHING) continue;

                    ent->posX++;
                }
            }

            if(impTimer == 0) {
                for(byte i = 0; i < imps.size(); i++) {
                    Imp* ent = imps[i];
                    if(ent->posX > 0) {
                        if(!shots.tryPush(Shot(ent->posX - 1, ent->posY, false, false))) {
#if CONF_DEBUG_LOG_PUSH_FAILURES
                            DEBUG_LOG(F("tryPush Shot failed (Imp)"));
#endif
                        }
                    }
                }
            }
        }
    };


    uint16_t globalSeed = 0;

}

class IntroScene : public Scene {
public:
    IntroScene() {
        frame.clear(' ');
    }

    void setInputs(Buttons held, Buttons pressed, Buttons released) {
        buttonsHeld = held;
        buttonsPressed = pressed;
        buttonsReleased = released;
    }

    void process() {
        if(buttonsHeld.down) {
            timer = 0;
            pause = 0;
        }
        if(buttonsReleased.right) {
            switchToGame(game::globalSeed);
            return;
        }

        if(pause > 0) {
            *frame.index(cursorX, cursorY) = (pause % 10) < 5 ? '_' : ' ';
            pause--;
            return;
        }

        if(text[textIndex] == '\0') {
            switchToGame(game::globalSeed);
            return;
        }

        if(timer <= 0) {
            bool lineEnded = false;
            if(text[textIndex] != '\n') lineEnded = write(text[textIndex++]);

            if(text[textIndex] == '\n') {
                textIndex++;

                pause = 20;
                if(!lineEnded) crlf();
                return;
            }

            timer = 2;
        } else {
            timer--;
        }
    }

    void draw(::LiquidCrystal* lcd_p) {
        frame.present(lcd_p);
    }

    void suspend() {}
    void resume(::LiquidCrystal* lcd_p) {}

private:
    Buttons buttonsHeld = {};
    Buttons buttonsPressed = {};
    Buttons buttonsReleased = {};
    gfx::Frame frame {};

    FlashStringWrapper text = FlashStringWrapper(::text::INTRO_F);
    unsigned int textIndex = 0;

    byte timer = 0;
    byte cursorX = 0;
    byte cursorY = 0;
    byte pause = 0; // Hatásszünet Változó

    bool write(char ch) {
        *frame.index(cursorX, cursorY) = ch;
        cursorX++;

        bool lineEnded = cursorX >= LCD_WIDTH;
        if(lineEnded) crlf();

        *frame.index(cursorX, cursorY) = '_';

        return lineEnded;
    }

    void crlf() {
        if(cursorX < LCD_WIDTH) *frame.index(cursorX, cursorY) = ' ';

        cursorX = 0;
        cursorY++;
        while(cursorY >= LCD_HEIGHT) {
            frame.scrollUp();
            cursorY--;
        }
    }
};

class MenuScene : public Scene {

public:

    MenuScene() {
        Random rand { 1 };
        for(byte i = 0; i < FIRE_COUNT; i++) {
            fires[i].randomizeMasks(&rand);
        }
    }

    void setInputs(Buttons held, Buttons pressed, Buttons released) {
        buttonsHeld = held;
        buttonsPressed = pressed;
        buttonsReleased = released;
    }

    virtual void process() {
        if(buttonsReleased.right) {
            switchToIntro();
        }

        for(byte i = 0; i < FIRE_COUNT; i++) {
            fires[i].advancePhase();
        }
    }

    virtual void draw(::LiquidCrystal* lcd_p) {
        gfx::Frame frame {};
        frame.clear(' ');


        byte composedFire[CHAR_HEIGHT];
        for(byte i = 0; i < FIRE_COUNT; i++) {
            fires[i].compose(composedFire);
            lcd_p->createChar(i, composedFire);
        }

        for(byte i = 0; i < LCD_WIDTH; i++) {
            *frame.index(i, LCD_HEIGHT - 1) = i % FIRE_COUNT;
        }


        const FlashStringWrapper title = FlashStringWrapper(F("ULTRAKRILL"));
        frame.write(3, 0, &title);


        frame.present(lcd_p);
    }

    virtual void suspend() {};
    virtual void resume(::LiquidCrystal* lcd_p) {};

private:
    Buttons buttonsHeld = {};
    Buttons buttonsPressed = {};
    Buttons buttonsReleased = {};
    const static byte FIRE_COUNT = 4;
    gfx::Fire fires[FIRE_COUNT];
};


Scene* scene_p = NULL;
Scene* nextScene_p = NULL;

MenuScene menuScene {};
game::Game gameScene { 0 };
IntroScene introScene {};
game::SkullScene skullScene { game::Stats() };


void switchToMenu() {
    menuScene.~MenuScene();
    nextScene_p = new(&menuScene) MenuScene;
}

void switchToIntro() {
    introScene.~IntroScene();
    nextScene_p = new(&introScene) IntroScene;
}

void switchToGame(uint16_t seed) {
    gameScene.~Game();
    nextScene_p = new(&gameScene) game::Game(game::globalSeed);
}

void switchToSkull(game::Stats stats) {
    skullScene.~SkullScene();
    nextScene_p = new(&skullScene) game::SkullScene(stats);
}



void setup() {
    Serial.begin(9600);
    lcd.begin(LCD_WIDTH, LCD_HEIGHT);
    DEBUG_LOG(F("LCD began"));

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
    DEBUG_LOG(F("Pin modes set"));

    game::createDefaultPatterns(&game::defaultPatterns);
    DEBUG_LOG_CAPTIONED(F("Patterns allocated"), MILLIS_PER_FRAME);
    switchToMenu();

    DEBUG_LOG_CAPTIONED(F("Frame length: "), MILLIS_PER_FRAME);
    nextFrameDue = millis() + MILLIS_PER_FRAME;
}


Buttons lastHeldButtons;
unsigned long worstSlack = 99999999;

void loop() {
    game::globalSeed++;

    // Jelenetváltás
    if(nextScene_p != NULL) {
        if(scene_p != NULL) {
            scene_p->suspend();
        }
        scene_p = nextScene_p;
        nextScene_p = NULL;
        lcd.clear();
        scene_p->resume(&lcd);
    }


    if(scene_p != NULL) {
        // Bemenet
        Buttons heldNow {
            .up = (digitalRead(PIN_BUTTON_UP) == LOW),
            .down = (digitalRead(PIN_BUTTON_DOWN) == LOW),
            .right = (digitalRead(PIN_BUTTON_RIGHT) == LOW)
        };

        scene_p->setInputs(heldNow, !lastHeldButtons && heldNow, lastHeldButtons && !heldNow);

        lastHeldButtons = heldNow;


        scene_p->process();
        // Ha időhiányban szenvedünk, akkor átugorjuk a kirajzolást, mert ez jelentős időbe telik, és "nem hat" a játékmenetre.
        if(!lagging) {
            scene_p->draw(&lcd);
        }
    }
#ifdef LCD_EMULATOR
    lcd.redraw();
#endif


    // Kitaláljuk, mennyit kell várni a következő frame elejéig
    unsigned long now = millis();
    lagging = now > nextFrameDue;
    if(!lagging) {
        unsigned long slack = nextFrameDue - now;

        digitalWrite(LED_BUILTIN, HIGH);
        delay(slack);
        digitalWrite(LED_BUILTIN, LOW);

        if(slack < worstSlack
#if CONF_DEBUG_ALWAYS_LOG_TIME
            || true
#endif
        ) {
            DEBUG_LOG_CAPTIONED(F("Slack: "), slack);
            worstSlack = slack;
        }
    } else {
        DEBUG_LOG_CAPTIONED(F("Lag: "), now - nextFrameDue);

#if CONF_PANIC_LAG
        panic(PANIC_LAG, F("We are lagging :("));
#endif
    }
    nextFrameDue += MILLIS_PER_FRAME;
}
