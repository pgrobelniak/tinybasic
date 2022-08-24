#include <SDL.h>

#include "scancodes.h"
#include "vt52rom.h"

#define TERM_WIDTH 80
#define TERM_HEIGHT 24

#define SCALE 1

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 20

#define WINDOW_WIDTH TERM_WIDTH*CHAR_WIDTH // 640
#define WINDOW_HEIGHT TERM_HEIGHT*CHAR_HEIGHT // 480

typedef struct Pen Pen;
struct Pen {
	Uint8 r, g, b
};

SDL_Window *term_window;
SDL_Renderer *term_renderer;
SDL_Texture *term_font[128];
SDL_Texture *term_canvas;
char term_chars[TERM_HEIGHT][TERM_WIDTH];
Pen term_colors[TERM_HEIGHT][TERM_WIDTH];
int term_curx = 0;
int term_cury = 0;
int term_shift = 0;
int term_ctrl = 0;
int term_blink = 0;
Uint32 term_userev;
Pen term_pen;
volatile int term_interactive = 0;
char term_lastkey = 0;

int term_run = 1;

int blink_thread(void *arg) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = term_userev;
    while(term_run) {
        term_blink = !term_blink;
        SDL_Delay(500);
        if (term_interactive) {
            SDL_PushEvent(&ev);
        }
    }
    return 0;
}

void term_setup() {
    SDL_Init(SDL_INIT_VIDEO);
    if(SDL_CreateWindowAndRenderer(WINDOW_WIDTH*SCALE, WINDOW_HEIGHT*SCALE, SDL_WINDOW_OPENGL, &term_window, &term_renderer) < 0) {
        fprintf(stderr, "%s\n", SDL_GetError());
        exit(1);
    }
    SDL_SetWindowTitle(term_window, "Basic");
    term_canvas = SDL_CreateTexture(term_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WINDOW_WIDTH, WINDOW_HEIGHT);
    SDL_SetRenderTarget(term_renderer, term_canvas);
    SDL_SetRenderDrawColor(term_renderer, 0, 0, 255, 255);
    SDL_RenderClear(term_renderer);
    SDL_SetRenderTarget(term_renderer, NULL);
    term_userev = SDL_RegisterEvents(1);
    SDL_CreateThread(blink_thread, "Blink", (void*)NULL);
    create_font();
    term_pen.r = 255;
    term_pen.g = 255;
    term_pen.b = 0;
    term_clear();
}

void term_putchar(char key) {
    if (key == 12) {
        term_clear();
    } else if (key == '\n') {
        term_curx=0;
        term_cury++;
        if (term_cury == TERM_HEIGHT) {
            scroll();
            term_cury = TERM_HEIGHT-1;
        }
    } else if (key == '\b') {
        handle_backspace();
    } else {
        term_chars[term_cury][term_curx] = key;
        term_colors[term_cury][term_curx] = term_pen;
        move_cursor_right();
    }
    draw();
}

short serialcheckch() {
    SDL_Event ev;
    if (!term_run) {
        term_lastkey = '#';
    } if(SDL_PollEvent(&ev) != 0) {
        switch(ev.type) {
            case SDL_QUIT:
                term_run = 0;
                term_lastkey = '#';
                break;
            case SDL_KEYDOWN:
                if (!handle_control_keydown(ev.key.keysym.scancode)) {
                    term_lastkey = decode_scancode(ev.key.keysym.scancode);
                }
                break;
            case SDL_KEYUP:
                handle_control_keyup(ev.key.keysym.scancode);
                break;
        }
    }
    return term_lastkey;
}

char serialread() {
    char key = term_lastkey;
    if (key == 0) {
        key = serialcheckch();
    }
    term_lastkey = 0;
    return key;
}

void consins(char *buffer, short nb) {
    term_interactive = 1;
    SDL_Event ev;
    while(term_interactive && term_run && SDL_WaitEvent(&ev) >= 0) {
        switch(ev.type){
            case SDL_QUIT:
                term_run = 0;
                break;
            case SDL_KEYDOWN:
                SDL_Scancode scancode = ev.key.keysym.scancode;
                if (!handle_control_keydown(scancode) && !handle_cursor_keys(scancode)) {
                    char key = decode_scancode(scancode);
                    handle_interactive_mode(key, buffer, nb);
                }
                break;
            case SDL_KEYUP:
                handle_control_keyup(ev.key.keysym.scancode);
                break;
        }
        draw();
    }
}

void rgbcolor(int r, int g, int b) {
    term_pen.r = r;
    term_pen.g = g;
    term_pen.b = b;
}

void frect(int x0, int y0, int x1, int y1)  {
    SDL_Rect rect;
    rect.x = x0;
    rect.y = y0;
    rect.w = x1 - x0;
    rect.h = y1 - y0;
    SDL_SetRenderTarget(term_renderer, term_canvas);
    SDL_SetRenderDrawColor(term_renderer, term_pen.r, term_pen.g, term_pen.b, 255);
    SDL_RenderFillRect(term_renderer, &rect);
    SDL_SetRenderTarget(term_renderer, NULL);
    draw();
}

void fcircle(int x0, int y0, int r) {
    SDL_SetRenderTarget(term_renderer, term_canvas);
    SDL_SetRenderDrawColor(term_renderer, term_pen.r, term_pen.g, term_pen.b, 255);
    for (int w = 0; w < r * 2; w++) {
        for (int h = 0; h < r * 2; h++) {
            int dx = r-w;
            int dy = r-h;
            if ((dx*dx+dy*dy) <= (r*r)) {
                SDL_RenderDrawPoint(term_renderer, x0 + dx, y0 + dy);
            }
        }
    }
    SDL_SetRenderTarget(term_renderer, NULL);
    draw();
}

void create_font() {
    Uint32 *raster = malloc(CHAR_WIDTH*CHAR_HEIGHT*sizeof(Uint32));
    for(int i = 0; i < 128; i++) {
        create_char(raster, i);
        term_font[i] = SDL_CreateTexture(term_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, CHAR_WIDTH, CHAR_HEIGHT);
        SDL_SetTextureBlendMode(term_font[i], SDL_BLENDMODE_ADD);
        SDL_UpdateTexture(term_font[i], NULL, raster, CHAR_WIDTH*sizeof(Uint32));
        SDL_SetTextureBlendMode(term_font[i], SDL_BLENDMODE_BLEND);
    }
}

void create_char(Uint32 *raster, int c) {
    Uint8 *chr = &vt52rom[c*8];
    memset(raster, 0, CHAR_WIDTH*CHAR_HEIGHT*sizeof(Uint32));
    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 7; j++) {
            if(chr[i]&(0100>>j)) {
                raster[(i*2)*CHAR_WIDTH+j]=0xFFFFFFFF;
                raster[(i*2+1)*CHAR_WIDTH+j]=0xFFFFFFFF;
            }
        }
    }
}

void term_clear() {
    term_curx = 0;
    term_cury = 0;
    for(int x = 0; x < TERM_WIDTH; x++) {
        for(int y = 0; y < TERM_HEIGHT; y++) {
            term_chars[y][x] = ' ';
            term_colors[y][x] = term_pen;
        }
    }
}

void draw() {
    int x, y, c;
    Pen p;
    SDL_Rect r;
    r.x = 0;
    r.y = 0;
    r.w = CHAR_WIDTH*SCALE;
    r.h = CHAR_HEIGHT*SCALE;
    SDL_SetRenderDrawColor(term_renderer, 255, 255, 255, 255);
    SDL_RenderCopy(term_renderer, term_canvas, NULL, NULL);
    SDL_SetRenderDrawColor(term_renderer, term_pen.r, term_pen.g, term_pen.b, 255);
    for(x = 0; x < TERM_WIDTH; x++) {
        for(y = 0; y < TERM_HEIGHT; y++) {
            c = term_chars[y][x];
            p = term_colors[y][x];
            if(c < 128) {
                r.x = (x*CHAR_WIDTH*SCALE);
                r.y = (y*CHAR_HEIGHT*SCALE);
                SDL_SetTextureColorMod(term_font[c], p.r, p.g, p.b);
                SDL_RenderCopy(term_renderer, term_font[c], NULL, &r);
            }
            if (term_interactive && term_blink && x == term_curx && y == term_cury) {
                r.x = (x*CHAR_WIDTH*SCALE);
                r.y = (y*CHAR_HEIGHT*SCALE);
                SDL_RenderFillRect(term_renderer, &r);
            }
        }
    }
    SDL_RenderPresent(term_renderer);
}

void scroll() {
    for(int y = 1; y < TERM_HEIGHT; y++) {
        for(int x = 0; x < TERM_WIDTH; x++) {
            term_chars[y-1][x] = term_chars[y][x];
            term_colors[y-1][x] = term_colors[y][x];
        }
    }
    for(int x = 0; x < TERM_WIDTH; x++) {
        term_chars[TERM_HEIGHT-1][x] = ' ';
        term_colors[TERM_HEIGHT-1][x] = term_pen;
    }
}

void move_cursor_left() {
    term_curx--;
    if (term_curx < 0) {
        term_curx = TERM_WIDTH - 1;
        term_cury--;
        if (term_cury < 0) {
            term_cury = 0;
            term_curx = 0;
        }
    }
}

void handle_backspace() {
    term_curx--;
    if (term_curx < 0) {
        move_cursor_left();
        term_chars[term_cury][term_curx] = ' ';
        term_colors[term_cury][term_curx] = term_pen;
    } else {
        for (int x = term_curx; x < TERM_WIDTH; x++) {
            if (x == (TERM_WIDTH-1)) {
                term_chars[term_cury][x] = ' ';
                term_colors[term_cury][x] = term_pen;
            } else {
                term_chars[term_cury][x] = term_chars[term_cury][x+1];
                term_colors[term_cury][x] = term_colors[term_cury][x+1];
            }
        }
    }
}

void move_cursor_up() {
    term_cury--;
    if (term_cury < 0) {
        term_cury = 0;
    }
}

void move_cursor_down() {
    term_cury++;
    if (term_cury == TERM_HEIGHT) {
        scroll();
        term_cury = TERM_HEIGHT-1;
    }
}

void move_cursor_right() {
    term_curx++;
    if (term_curx == TERM_WIDTH) {
        term_curx = 0;
        move_cursor_down();
    }
}

int handle_control_keydown(SDL_Scancode scancode) {
    switch(scancode) {
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:
            term_shift = 1;
            return 1;
        case SDL_SCANCODE_CAPSLOCK:
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            term_ctrl = 1;
            return 1;
    }
    return 0;
}

int handle_cursor_keys(SDL_Scancode scancode) {
    switch(scancode) {
        case SDL_SCANCODE_LEFT:
            move_cursor_left();
            return 1;
        case SDL_SCANCODE_RIGHT:
            move_cursor_right();
            return 1;
        case SDL_SCANCODE_UP:
            move_cursor_up();
            return 1;
        case SDL_SCANCODE_DOWN:
            move_cursor_down();
            return 1;
    }
    return 0;
}

int decode_scancode(SDL_Scancode scancode) {
    char *keys = scancodemap[scancode];
    if(keys == NULL) {
        return 0;
    }
    char key = keys[term_shift];
    if(term_ctrl) {
        key &= 037;
    }
    return key;
}

void handle_interactive_mode(char key, char *b, short nb) {
    if (key == '\n') {
        char c;
        z.a=1;
        while(z.a < nb && z.a < TERM_WIDTH) {
            c=term_chars[term_cury][z.a-1];
            b[z.a++]=c;
        }
        b[z.a]=0x00;
        z.a--;
        b[0]=(unsigned char)z.a;
        term_interactive = 0;
    }
    term_putchar(key);
}

void handle_control_keyup(SDL_Scancode scancode) {
    switch(scancode) {
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:
            term_shift = 0;
            return;
        case SDL_SCANCODE_CAPSLOCK:
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            term_ctrl = 0;
            return;
    }
}




