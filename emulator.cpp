#include <iostream>
#include <string>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>


class LiquidCrystal {

public:

    SDL_Color colorBlank = SDL_Color { .r =  0x2d, .g = 0x60, .b = 0x1f, .a = 255 };
    SDL_Color colorOn = SDL_Color { .r = 0x49, .g = 0xe0, .b = 0x1f, .a = 255 };
    SDL_Color colorOff = SDL_Color { .r = 0x30, .g = 0x70, .b = 0x1f, .a = 255 };


    LiquidCrystal(unsigned char bogus0, unsigned char bogus1, unsigned char bogus2, unsigned char bogus3, unsigned char bogus4, unsigned char bogus5) {

        if(!SDL_CreateWindowAndRenderer("lcd emulator", SCALE * (CHAR_WIDTH * WIDTH + 2 * MARGIN + (WIDTH - 1) * (GAP_HORIZONTAL)), SCALE * (CHAR_HEIGHT * HEIGHT + 2 * MARGIN + (HEIGHT - 1) * GAP_VERTICAL), 0, &window, &renderer)) {
            SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        }
        if(!SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED)) {
            SDL_Log("Couldn't center window: %s", SDL_GetError());
        }

        SDL_Surface *fontSurface = IMG_Load("font.png");
        if(fontSurface == NULL) {
            SDL_Log("Couldn't load font.png: %s", SDL_GetError());
        }

        font = SDL_CreateTextureFromSurface(renderer, fontSurface);
        if(font == NULL) {
            SDL_Log("Couldn't create texture: %s", SDL_GetError());
        }
        if(!SDL_SetTextureBlendMode(font, SDL_BLENDMODE_BLEND)) {
            SDL_Log("Couldn't set blend mode: %s", SDL_GetError());
        }
        if(!SDL_SetTextureScaleMode(font, SDL_SCALEMODE_NEAREST)) {
            SDL_Log("Couldn't set scale mode: %s", SDL_GetError());
        }

        if(!SDL_SetRenderScale(renderer, SCALE, SCALE)) {
            SDL_Log("Couldn't set scale: %s", SDL_GetError());
        }

        clear();
    }

    ~LiquidCrystal() {
        SDL_DestroyTexture(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_PumpEvents();
    }

    void begin(unsigned char width, unsigned char height) {
        // do nothing
    }

    void clear() {
        for(int i = 0; i < WIDTH * HEIGHT; i++) buffer[i] = ' ';
        cursorX = 0;
        cursorY = 0;
    }

    void setCursor(unsigned char x, unsigned char y) {
        cursorX = x;
        cursorY = y;
    }

    void write(unsigned char val) {
        if(cursorX < 0 || cursorX >= WIDTH) return;
        if(cursorY < 0) cursorY = 0;
        else if(cursorY >= HEIGHT) cursorY = HEIGHT - 1;

        buffer[(cursorY * WIDTH) + cursorX] = val;
        cursorX++;
    }

    void print(const char* text) {
        while(text) {
            write(*text);
            text++;
        }
    }

    void createChar(unsigned char index, unsigned char data[8]) {
        for(int i = 0; i < CHAR_HEIGHT; i++) customChars[index][i] = data[i];
    }

    void redraw() {
        setColor(colorBlank);
        SDL_RenderClear(renderer);

        if(!SDL_SetTextureColorMod(this->font, colorOn.r, colorOn.g, colorOn.b)) {
            SDL_Log("Couldn't set color mod: %s", SDL_GetError());
        }


        setColor(colorOff);
        for(int y = 0; y < HEIGHT; y++) {
            for(int x = 0; x < WIDTH; x++) {
                unsigned char here = buffer[(y * WIDTH) + x];

                int dstX = MARGIN + x * (CHAR_WIDTH + GAP_HORIZONTAL);
                int dstY = MARGIN + y * (CHAR_HEIGHT + GAP_VERTICAL);
                SDL_FRect dst = SDL_FRect {
                    .x = static_cast<float>(dstX),
                    .y = static_cast<float>(dstY),
                    .w = CHAR_WIDTH,
                    .h = CHAR_HEIGHT
                };

                SDL_RenderFillRect(renderer, &dst);
                if(here >= 8) {
                    int col = here % ATLAS_WIDTH;
                    int row = here / ATLAS_WIDTH;
                    SDL_FRect src = SDL_FRect {
                        .x = static_cast<float>(col * CHAR_WIDTH),
                        .y = static_cast<float>(row * CHAR_HEIGHT),
                        .w = CHAR_WIDTH,
                        .h = CHAR_HEIGHT
                    };
                    SDL_RenderTexture(renderer, font, &src, &dst);
                } else {
                    for(int j = 0; j < CHAR_HEIGHT; j++) {
                        for(int i = 0; i < CHAR_WIDTH; i++) {
                            bool bit = (customChars[here][j] & (1 << (CHAR_WIDTH - 1 - i))) > 0;
                            setColor(bit ? colorOn : colorOff);
                            SDL_RenderPoint(renderer, dstX + i, dstY + j);
                        }
                    }
                    setColor(colorOff);
                }

            }
        }

        SDL_RenderPresent(renderer);
    }

private:

    const static int SCALE = 4;
    const static int MARGIN = 4;
    const static int GAP_HORIZONTAL = 1;
    const static int GAP_VERTICAL = 2;
    const static int ATLAS_WIDTH = 8;

    const static int WIDTH = 16;
    const static int HEIGHT = 2;
    const static int CHAR_WIDTH = 5;
    const static int CHAR_HEIGHT = 8;
    unsigned char buffer[WIDTH * HEIGHT];

    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *font = NULL;

    unsigned char customChars[8][8];
    int cursorX = 0;
    int cursorY = 0;

    void setColor(SDL_Color col) {
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
    }

};


int digitalRead(unsigned char pin) {
    const bool* keyboard = SDL_GetKeyboardState(NULL);
    switch(pin) {
        case 8: return !(keyboard[SDL_SCANCODE_DOWN] || keyboard[SDL_SCANCODE_S]);
        case 9: return !(keyboard[SDL_SCANCODE_UP] || keyboard[SDL_SCANCODE_W]);
        case 10: return !(keyboard[SDL_SCANCODE_RIGHT] || keyboard[SDL_SCANCODE_SPACE]);
    }
    return 0;
}

void digitalWrite(unsigned char pin, unsigned char val) {
    // do nothing
}

void pinMode(unsigned char pin, unsigned char mode) {
    // do nothing
}

struct HardwareSerial {
    static void begin(unsigned long) {
        // do nothing
    }

    static void print(const char* text) {
        std::cout << text;
    }
    static void println() {
        std::cout << '\n';
    }
    static void println(const char* text) {
        std::cout << text;
        std::cout << '\n';
    }
    static void print(const char ch) {
        std::cout << ch;
    }
    static void println(const unsigned char b) {
        std::cout << (int)b;
        std::cout << '\n';
    }
    static void print(const bool b) {
        std::cout << b;
    }
    static void println(const bool b) {
        std::cout << b;
        std::cout << '\n';
    }
    static void print(const unsigned int b) {
        std::cout << b;
    }
    static void println(const unsigned int b) {
        std::cout << b;
        std::cout << '\n';
    }
    static void print(const unsigned long b) {
        std::cout << b;
    }
    static void println(const unsigned long b) {
        std::cout << b;
        std::cout << '\n';
    }
};

class String {

    public:
        String() : str() { /* do nothing */ }
        String(const char* s) : str(s) { /* do nothing */ }

        String operator +=(const char* text) {
            str += text;
            return *this;
        }
        String operator +=(char ch) {
            str += std::to_string(ch);
            return *this;
        }
        String operator +=(int n) {
            str += std::to_string(n);
            return *this;
        }

        const char* c_str() {
            return str.c_str();
        }

        unsigned int length() const {
            return str.length();
        }

        char operator[](unsigned int index) const {
            return str[index];
        }

    private:
        std::string str;
};

unsigned long millis() {
    return SDL_GetTicks();
}

void delay(unsigned long ms) {
    SDL_Delay(ms);
}

long random(long maxExcl) {
    return SDL_rand(maxExcl);
}



#define LCD_EMULATOR
typedef unsigned char byte;
#define LED_BUILTIN 13
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define min std::min
#define max std::max
#define isLowerCase(x) (tolower(x) == x)
#define F(x) x
HardwareSerial Serial;
#include "ultrakrill/ultrakrill.ino"
#undef LCD_EMULATOR


int main() {
    SDL_SetAppMetadata("ULTRAKRILL Emulator", "1.0", NULL);

    if(!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return 1;
    }


    setup();

    bool run = true;
    while(run) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_EVENT_QUIT:
                    run = false;
                    break;
            }
        }

        loop();
    }
}
