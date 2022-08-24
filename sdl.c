#include <stdlib.h>
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

typedef struct Col Col;
struct Col {
	Uint8 a, b, g, r;
};

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *fonttex[128];
SDL_Texture *canvas;
char fb[TERM_HEIGHT][TERM_WIDTH];
Col fbc[TERM_HEIGHT][TERM_WIDTH];
int curx = 0;
int cury = 0;
int shift = 0;
int ctrl = 0;
int blink = 0;
Uint32 userevent;
int run = 1;
int pr = 255;
int pg = 255;
int pb = 0;
int br = 0;
int bg = 0;
int bb = 255;
volatile int interactive = 0;

int term_running() {
    return run;
}

void createChar(Uint32 *raster, int c) {
    int i, j;
    Uint8 *chr = &vt52rom[c*8];
    memset(raster, 0, CHAR_WIDTH*CHAR_HEIGHT*sizeof(Uint32));
    Col *d = (Col*)raster;
    for(i = 0; i < 8; i++) {
        for(j = 0; j < 7; j++) {
            if(chr[i]&(0100>>j)) {
                Col col;
                col.r = 255;
                col.g = 255;
                col.b = 255;
                col.a = 255;
                d[(i*2)*CHAR_WIDTH + j] = col;
                d[(i*2+1)*CHAR_WIDTH + j] = col;
                //d[i*CHAR_WIDTH + j] = col;
            }
        }
    }
}

void createFont() {
    int w = CHAR_WIDTH;
    int h = CHAR_HEIGHT;
    Uint32 *raster = malloc(w*h*sizeof(Uint32));
    for(int i = 0; i < 128; i++) {
        createChar(raster, i);
        fonttex[i] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        SDL_SetTextureBlendMode(fonttex[i], SDL_BLENDMODE_ADD);
        SDL_UpdateTexture(fonttex[i], NULL, raster, w*sizeof(Uint32));
        SDL_SetTextureBlendMode(fonttex[i], SDL_BLENDMODE_BLEND);
    }
}

int blinkThread(void *arg) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = userevent;
    while(run) {
        blink = !blink;
        SDL_Delay(500);
        if (interactive) {
            SDL_PushEvent(&ev);
        }
    }
    return 0;
}

void term_clear() {
    curx = 0;
    cury = 0;
    for(int x = 0; x < TERM_WIDTH; x++) {
        for(int y = 0; y < TERM_HEIGHT; y++) {
            fb[y][x] = ' ';
            fbc[y][x].r = pr;
            fbc[y][x].g = pg;
            fbc[y][x].b = pb;
        }
    }
}

void term_setup() {
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
    if(SDL_CreateWindowAndRenderer(WINDOW_WIDTH * SCALE, WINDOW_HEIGHT * SCALE, flags, &window, &renderer) < 0) {
        fprintf(stderr, "%s\n", SDL_GetError());
        exit(1);
    }
    SDL_SetWindowTitle(window, "Vortex");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_SetHint("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS", "0");
    createFont();
    userevent = SDL_RegisterEvents(1);
    canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WINDOW_WIDTH, WINDOW_HEIGHT);
    SDL_SetRenderTarget(renderer, canvas);
    SDL_SetRenderDrawColor(renderer, br, bg, bb, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, NULL);
    term_clear();
    SDL_CreateThread(blinkThread, "Blink", (void*)NULL);
}

void draw() {
    int x, y, c;
    SDL_Rect r;
    r.x = 0;
    r.y = 0;
    r.w = CHAR_WIDTH * SCALE;
    r.h = CHAR_HEIGHT * SCALE;
    SDL_SetRenderDrawColor(renderer, br, bg, bb, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderCopy(renderer, canvas, NULL, NULL);
    SDL_SetRenderDrawColor(renderer, pr, pg, pb, 255);
    for(x = 0; x < TERM_WIDTH; x++) {
        for(y = 0; y < TERM_HEIGHT; y++) {
            c = fb[y][x];
            if(c < 128) {
                r.x = (x * CHAR_WIDTH * SCALE);
                r.y = (y * CHAR_HEIGHT * SCALE);
                SDL_SetTextureColorMod(fonttex[c], fbc[y][x].r, fbc[y][x].g, fbc[y][x].b);
                SDL_RenderCopy(renderer, fonttex[c], NULL, &r);
            }
            if (blink && x == curx && y == cury) {
                r.x = (x * CHAR_WIDTH * SCALE);
                r.y = (y * CHAR_HEIGHT * SCALE);
                SDL_RenderFillRect(renderer, &r);
            }
        }
    }
    SDL_RenderPresent(renderer);
}

void rgbcolor(int r, int g, int b) {
    pr = r;
    pg = g;
    pb = b;
}

void frect(int x0, int y0, int x1, int y1)  {
    SDL_Rect rect;
    rect.x = x0;
    rect.y = y0;
    rect.w = x1 - x0;
    rect.h = y1 - y0;
    SDL_SetRenderTarget(renderer, canvas);
    SDL_SetRenderDrawColor(renderer, pr, pg, pb, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderTarget(renderer, NULL);
    draw();
}

void scroll() {
    int x, y;
    for(y = 1; y < TERM_HEIGHT; y++) {
        for(x = 0; x < TERM_WIDTH; x++) {
            fb[y-1][x] = fb[y][x];
            fbc[y-1][x] = fbc[y][x];
        }
    }
    for(x = 0; x < TERM_WIDTH; x++) {
        fb[TERM_HEIGHT-1][x] = ' ';
        fbc[TERM_HEIGHT-1][x].r = pr;
        fbc[TERM_HEIGHT-1][x].g = pg;
        fbc[TERM_HEIGHT-1][x].b = pb;
    }
}

void toggleFullscreen(void) {
    Uint32 f = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_SetWindowFullscreen(window, f ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void moveLeft() {
    curx--;
    if (curx < 0) {
        curx = TERM_WIDTH - 1;
        cury--;
        if (cury<0) {
            cury = 0;
            curx = 0;
        }
    }
}

void backSpace() {
    curx--;
    if (curx < 0) {
        moveLeft();
        fb[cury][curx] = ' ';
        fbc[cury][curx].r = pr;
        fbc[cury][curx].b = pb;
        fbc[cury][curx].g = pg;
    } else {
        for (int x = curx; x < TERM_WIDTH; x++) {
            if (x == (TERM_WIDTH-1)) {
                fb[cury][x] = ' ';
                fbc[cury][x].r = pr;
                fbc[cury][x].g = pg;
                fbc[cury][x].b = pb;
            } else {
                fb[cury][x] = fb[cury][x+1];
                fbc[cury][x].r = fbc[cury][x+1].r;
                fbc[cury][x].g = fbc[cury][x+1].g;
                fbc[cury][x].b = fbc[cury][x+1].b;
            }
        }
    }
}

void moveUp() {
    cury--;
    if (cury < 0) {
        cury = 0;
    }
}

void moveDown() {
    cury++;
    if (cury == TERM_HEIGHT) {
        scroll();
        cury = TERM_HEIGHT-1;
    }
}

void moveRight() {
    curx++;
    if (curx == TERM_WIDTH) {
        curx = 0;
        moveDown();
    }
}

int enter = -1;

void term_putchar(char key) {
    if (key == 12) {
        term_clear();
    } else if (key == '\n') {
        curx=0;
        cury++;
        if (cury == TERM_HEIGHT) {
            scroll();
            cury = TERM_HEIGHT-1;
        }
    } else if (key == '\b') {
        backSpace();
    } else {
        fb[cury][curx]=key;
        fbc[cury][curx].r=pr;
        fbc[cury][curx].g=pg;
        fbc[cury][curx].b=pb;
        moveRight();
    }
    draw();
}

void keydown(SDL_Scancode scancode, int repeat) {
    switch(scancode) {
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:
            shift = 1;
            return;
        case SDL_SCANCODE_CAPSLOCK:
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            ctrl = 1;
            return;
        case SDL_SCANCODE_RETURN:
            curx=0;
            cury++;
            if (cury == TERM_HEIGHT) {
                scroll();
                cury = TERM_HEIGHT-1;
            }
            enter = cury - 1;
            return;
        case SDL_SCANCODE_BACKSPACE:
            backSpace();
            return;
        case SDL_SCANCODE_LEFT:
            moveLeft();
            return;
        case SDL_SCANCODE_RIGHT:
            moveRight();
            return;
        case SDL_SCANCODE_UP:
            moveUp();
            return;
        case SDL_SCANCODE_DOWN:
            moveDown();
            return;
        default:
            break;
    }
    if(scancode == SDL_SCANCODE_F11 && !repeat) {
        toggleFullscreen();
    }
    char *keys = scancodemap[scancode];
    if(keys == NULL) {
        return;
    }
    char key = keys[shift];
    if(ctrl) {
        key &= 037;
    }
    term_putchar(key);
}

void keyup(SDL_Scancode scancode) {
    switch(scancode) {
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:
            shift = 0;
            return;
        case SDL_SCANCODE_CAPSLOCK:
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            ctrl = 0;
            return;
        default:
            break;
    }
}

void consins(char *b, short nb) {
    interactive = 1;
    SDL_Event ev;
    while(run && SDL_WaitEvent(&ev) >= 0) {
        switch(ev.type){
            case SDL_QUIT:
                run = 0;
                break;
            case SDL_KEYDOWN:
                keydown(ev.key.keysym.scancode, ev.key.repeat);
                break;
            case SDL_KEYUP:
                keyup(ev.key.keysym.scancode);
                break;
        }
        draw();
        if (enter != -1) {
            b[0] = TERM_WIDTH;
            for (int i = 0; i < TERM_WIDTH; i++) {
                b[i+1]=fb[enter][i];
            }
            b[TERM_WIDTH+2]=0;
            enter = -1;
            break;
        }
    }
    interactive = 0;
}
