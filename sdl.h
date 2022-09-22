/*
 * SDL display driver for Stefan's basic interpreter
 *
 * Author: Pawel Grobelniak, pawel.grobelniak@gmail.com
 *
 */

#include <SDL.h>
#include "scancodes.h"
#include "petscii.h"

#define STANDALONE
#define DISPLAYDRIVER
#define HASKEYBOARD

#define dsp_columns 80
#define dsp_rows 60

int dspmycol = 0;
int dspmyrow = 0;

float dsp_scale = 1;
float dsp_offx = 0;
float dsp_offy = 0;

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

#define WINDOW_WIDTH dsp_columns*CHAR_WIDTH // 640
#define WINDOW_HEIGHT dsp_rows*CHAR_HEIGHT // 480

typedef struct {
    Uint8 r, g, b;
} Pen;

SDL_Window *term_window;
SDL_Renderer *term_renderer;
SDL_Texture *term_font[128];
SDL_Texture *term_cursor[128];
SDL_Texture *term_canvas;
char term_chars[dsp_rows][dsp_columns];
Pen term_colors[dsp_rows][dsp_columns];
int term_shift = 0;
int term_ctrl = 0;
int term_blink = 0;
Uint32 term_userev;
Pen term_pen;
volatile int term_interactive = 0;
char term_lastkey = 0;
char term_mode = 0;
const Uint8 *term_keystate;

/*
 * Internal functions
 */

int blink_thread(void *arg) {
    /*
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = term_userev;
    for(;;) {
        term_blink = !term_blink;
        SDL_Delay(500);
        if (term_interactive) {
            SDL_PushEvent(&ev);
        }
    }
    */
    return 0;
}


void create_char(Uint32 *raster, int c, int inv) {
    Uint8 *chr = &vt52rom[c*8];
    memset(raster, 0, CHAR_WIDTH*CHAR_HEIGHT*sizeof(Uint32));
    int fill;
    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 8; j++) {
            fill = chr[i]&(0100>>j);
            if (inv) fill = !fill;
            if(fill) {
                raster[i*CHAR_WIDTH+j]=0xFFFFFFFF;
            }
        }
    }
}

void create_font() {
    Uint32 *raster = malloc(CHAR_WIDTH*CHAR_HEIGHT*sizeof(Uint32));
    for(int i = 0; i < 128; i++) {
        create_char(raster, i, 0);
        term_font[i] = SDL_CreateTexture(term_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, CHAR_WIDTH, CHAR_HEIGHT);
        SDL_SetTextureBlendMode(term_font[i], SDL_BLENDMODE_ADD);
        SDL_UpdateTexture(term_font[i], NULL, raster, CHAR_WIDTH*sizeof(Uint32));
        SDL_SetTextureBlendMode(term_font[i], SDL_BLENDMODE_BLEND);
        create_char(raster, i, 1);
        term_cursor[i] = SDL_CreateTexture(term_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, CHAR_WIDTH, CHAR_HEIGHT);
        SDL_SetTextureBlendMode(term_cursor[i], SDL_BLENDMODE_ADD);
        SDL_UpdateTexture(term_cursor[i], NULL, raster, CHAR_WIDTH*sizeof(Uint32));
        SDL_SetTextureBlendMode(term_cursor[i], SDL_BLENDMODE_BLEND);
    }
}

void term_clear() {
    dspmycol = 0;
    dspmyrow = 0;
    for(int x = 0; x < dsp_columns; x++) {
        for(int y = 0; y < dsp_rows; y++) {
            term_chars[y][x] = ' ';
            term_colors[y][x] = term_pen;
        }
    }
}

void term_reset() {
    SDL_SetRenderDrawColor(term_renderer, 255, 255, 255, 255);
    SDL_RenderClear(term_renderer);
    SDL_SetRenderTarget(term_renderer, term_canvas);
    term_pen.r = 0;
    term_pen.g = 0;
    term_pen.b = 0;
    SDL_SetRenderDrawColor(term_renderer, term_pen.r, term_pen.g, term_pen.b, 255);
    term_clear();
    term_mode = 0;
}

void draw() {
    if (term_mode) return;
    int x, y;
    SDL_Rect r;
    r.x = dsp_offx;
    r.y = dsp_offy;
    r.w = WINDOW_WIDTH * dsp_scale;
    r.h = WINDOW_HEIGHT * dsp_scale;
    SDL_SetRenderTarget(term_renderer, NULL);
    SDL_SetRenderDrawColor(term_renderer, 0, 0, 0, 255);
    SDL_RenderClear(term_renderer);
    SDL_SetRenderDrawColor(term_renderer, term_pen.r, term_pen.g, term_pen.b, 255);
    SDL_RenderCopy(term_renderer, term_canvas, NULL, &r);
    SDL_RenderPresent(term_renderer);
    SDL_SetRenderTarget(term_renderer, term_canvas);
}

void redraw_text() {
    SDL_SetRenderDrawColor(term_renderer, 255, 255, 255, 0);
    SDL_RenderClear(term_renderer);
    int x, y, c;
    Pen p;
    SDL_Rect r;
    r.w = CHAR_WIDTH*dsp_scale;
    r.h = CHAR_HEIGHT*dsp_scale;
    for(x = 0; x < dsp_columns; x++) {
        for(y = 0; y < dsp_rows; y++) {
            c = term_chars[y][x];
            p = term_colors[y][x];
            if (c >= 128) c = 32;
            if(c < 128) {
                SDL_Texture *font = term_font[c];
                SDL_SetTextureColorMod(font, p.r, p.g, p.b);
                r.x = dsp_offx + (x*CHAR_WIDTH*dsp_scale);
                r.y = dsp_offy + (y*CHAR_HEIGHT*dsp_scale);
                SDL_RenderCopy(term_renderer, font, NULL, &r);
            }
        }
    }
    draw();
}

void draw_char(int x, int y, char c) {
    Pen p = term_colors[y][x];
    SDL_Rect r;
    r.w = CHAR_WIDTH*dsp_scale;
    r.h = CHAR_HEIGHT*dsp_scale;
    if (c >= 128) c = 32;
    if(c < 128) {
        SDL_Texture *font = term_font[c];
        SDL_SetTextureColorMod(font, p.r, p.g, p.b);
        r.x = dsp_offx + (x*CHAR_WIDTH*dsp_scale);
        r.y = dsp_offy + (y*CHAR_HEIGHT*dsp_scale);
        SDL_RenderCopy(term_renderer, font, NULL, &r);
    }
    draw();
}

void scroll() {
    for(int y = 1; y < dsp_rows; y++) {
        for(int x = 0; x < dsp_columns; x++) {
            term_chars[y-1][x] = term_chars[y][x];
            term_colors[y-1][x] = term_colors[y][x];
        }
    }
    for(int x = 0; x < dsp_columns; x++) {
        term_chars[dsp_rows-1][x] = ' ';
        term_colors[dsp_rows-1][x] = term_pen;
    }
}

void move_cursor_left() {
    dspmycol--;
    if (dspmycol < 0) {
        dspmycol = dsp_columns - 1;
        dspmyrow--;
        if (dspmyrow < 0) {
            dspmyrow = 0;
            dspmycol = 0;
        }
    }
}

void move_cursor_down() {
    dspmyrow++;
    if (dspmyrow == dsp_rows) {
        scroll();
        dspmyrow = dsp_rows-1;
    }
}

void move_cursor_right() {
    dspmycol++;
    if (dspmycol == dsp_columns) {
        dspmycol = 0;
        move_cursor_down();
    }
}

void move_cursor_up() {
    dspmyrow--;
    if (dspmyrow < 0) {
        dspmyrow = 0;
    }
}

void handle_backspace() {
    dspmycol--;
    if (dspmycol < 0) {
        move_cursor_left();
        term_chars[dspmyrow][dspmycol] = ' ';
        term_colors[dspmyrow][dspmycol] = term_pen;
    } else {
        for (int x = dspmycol; x < dsp_columns; x++) {
            if (x == (dsp_columns-1)) {
                term_chars[dspmyrow][x] = ' ';
                term_colors[dspmyrow][x] = term_pen;
            } else {
                term_chars[dspmyrow][x] = term_chars[dspmyrow][x+1];
                term_colors[dspmyrow][x] = term_colors[dspmyrow][x+1];
            }
        }
    }
}

void handle_insert() {
    for (int x = dsp_columns-1; x > dspmycol ; x--) {
        term_chars[dspmyrow][x] = term_chars[dspmyrow][x-1];
        term_colors[dspmyrow][x] = term_colors[dspmyrow][x-1];
    }
    term_chars[dspmyrow][dspmycol] = ' ';
}

void handle_home() {
    dspmycol=0;
}

void handle_end() {
    int x;
    for (dspmycol = dsp_columns-1; dspmycol >= 0; dspmycol--) {
        if (term_chars[dspmyrow][dspmycol] != ' ') {
            break;
        }
    }
    if (dspmycol < dsp_columns-1) {
        dspmycol++;
    }
}

void quit() {
    SDL_Quit();
    exit(0);
}

void handle_interactive_mode(char key, char *b, short nb) {
    if (key == '\n') {
        char c;
        z.a=1;
        while(z.a < nb && z.a < dsp_columns) {
            c=term_chars[dspmyrow][z.a-1];
            b[z.a++]=c;
        }
        b[z.a]=0x00;
        z.a--;
        b[0]=(unsigned char)z.a;
        term_interactive = 0;
    }
    dspwrite(key);
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
    case SDL_SCANCODE_F12:
        term_reset();
        redraw_text();
        if (!term_interactive) {
            term_lastkey = '#';
        }
        return 1;
    }
    return 0;
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
    case SDL_SCANCODE_INSERT:
        handle_insert();
        return 1;
    case SDL_SCANCODE_HOME:
        handle_home();
        return 1;
    case SDL_SCANCODE_END:
        handle_end();
        return 1;
    }
    return 0;
}

void calculate_scale(int w, int h) {
    float ws = w / ((float)WINDOW_WIDTH);
    float wh = h / ((float)WINDOW_HEIGHT);
    dsp_scale = ws < wh ? ws : wh;
    dsp_offx = (w - WINDOW_WIDTH * dsp_scale) / 2;
    dsp_offy = (h - WINDOW_HEIGHT * dsp_scale) / 2;
}

void handle_window_event(SDL_WindowEvent e) {
    switch(e.event) {
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        calculate_scale(e.data1, e.data2);
        break;
    }
}

/*
 * Display functions
 */

void dspbegin() {
    SDL_Init(SDL_INIT_VIDEO);
    term_keystate = SDL_GetKeyboardState(NULL);
    if(SDL_CreateWindowAndRenderer(WINDOW_WIDTH*dsp_scale, WINDOW_HEIGHT*dsp_scale, SDL_WINDOW_RESIZABLE, &term_window, &term_renderer) < 0) {
        fprintf(stderr, "%s\n", SDL_GetError());
        exit(1);
    }
    SDL_SetWindowTitle(term_window, "Basic");
    //SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    //SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
    term_canvas = SDL_CreateTexture(term_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WINDOW_WIDTH, WINDOW_HEIGHT);    
    term_userev = SDL_RegisterEvents(1);
    SDL_CreateThread(blink_thread, "Blink", (void*)NULL);
    create_font();
    SDL_SetRenderTarget(term_renderer, term_canvas);
    term_reset();
}

void dspwrite(char key) {
    if (key == 12) {
        term_clear();
        redraw_text();
    } else if (key == '\n') {
        dspmycol=0;
        dspmyrow++;
        if (dspmyrow == dsp_rows) {
            scroll();
            dspmyrow = dsp_rows-1;
        }
        redraw_text();
    } else if (key == '\b') {
        handle_backspace();
        redraw_text();
    } else {
        term_chars[dspmyrow][dspmycol] = key;
        term_colors[dspmyrow][dspmycol] = term_pen;
        draw_char(dspmycol, dspmyrow, key);
        move_cursor_right();
    }
}

void dspsetupdatemode(char c) {
    term_mode = c;
    if (!c) {
        draw();
    }
}

int dspticks() {
    return SDL_GetTicks();
}

void rgbcolor(int r, int g, int b) {
    term_pen.r = r;
    term_pen.g = g;
    term_pen.b = b;
    SDL_SetRenderDrawColor(term_renderer, r, g, b, 255);
}

void plot(int x, int y) {
    SDL_RenderDrawPoint(term_renderer, x, y);
    draw();
}

void line(int x0, int y0, int x1, int y1) {
    SDL_RenderDrawLine(term_renderer, x0, y0, x1, y1);
    draw();
}

void rect(int x0, int y0, int x1, int y1) {
    SDL_Rect rect;
    rect.x = x0;
    rect.y = y0;
    rect.w = x1 - x0;
    rect.h = y1 - y0;
    SDL_RenderDrawRect(term_renderer, &rect);
    draw();
}

void frect(int x0, int y0, int x1, int y1) {
    SDL_Rect rect;
    rect.x = x0;
    rect.y = y0;
    rect.w = x1 - x0;
    rect.h = y1 - y0;
    SDL_RenderFillRect(term_renderer, &rect);
    draw();
}

void circle(int x, int y, int radius) {
    int offsetx = 0;
    int offsety = radius;
    int d = radius -1;
    int status = 0;
    while (offsety >= offsetx) {
        status += SDL_RenderDrawPoint(term_renderer, x + offsetx, y + offsety);
        status += SDL_RenderDrawPoint(term_renderer, x + offsety, y + offsetx);
        status += SDL_RenderDrawPoint(term_renderer, x - offsetx, y + offsety);
        status += SDL_RenderDrawPoint(term_renderer, x - offsety, y + offsetx);
        status += SDL_RenderDrawPoint(term_renderer, x + offsetx, y - offsety);
        status += SDL_RenderDrawPoint(term_renderer, x + offsety, y - offsetx);
        status += SDL_RenderDrawPoint(term_renderer, x - offsetx, y - offsety);
        status += SDL_RenderDrawPoint(term_renderer, x - offsety, y - offsetx);
        if (status < 0) {
            break;
        }
        if (d >= 2*offsetx) {
            d -= 2*offsetx + 1;
            offsetx +=1;
        } else if (d < 2 * (radius - offsety)) {
            d += 2 * offsety - 1;
            offsety -= 1;
        } else {
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }
    draw();
}

void fcircle(int x0, int y0, int r) {
    for (int w = 0; w < r * 2; w++) {
        for (int h = 0; h < r * 2; h++) {
            int dx = r-w;
            int dy = r-h;
            if ((dx*dx+dy*dy) <= (r*r)) {
                SDL_RenderDrawPoint(term_renderer, x0 + dx, y0 + dy);
            }
        }
    }
    draw();
}

/*
 * Keyboard functions
 */

char kbdavailable() {
    return term_lastkey;
}

char kbdcheckch() {
    SDL_Event ev;
    if(SDL_PollEvent(&ev) != 0) {
        switch(ev.type) {
        case SDL_QUIT:
            quit();
            break;
        case SDL_KEYDOWN:
            if (!handle_control_keydown(ev.key.keysym.scancode)) {
                term_lastkey = decode_scancode(ev.key.keysym.scancode);
            }
            if (term_lastkey=='#') {
                dspsetupdatemode(0);
            }
            break;
        case SDL_KEYUP:
            handle_control_keyup(ev.key.keysym.scancode);
            break;
        case SDL_WINDOWEVENT:
            handle_window_event(ev.window);
            break;
        }
    }
    return term_lastkey;
}

char kbdread() {
    char key = term_lastkey;
    if (key == 0) {
        key = kbdcheckch();
    }
    term_lastkey = 0;
    return key;
}

int kbdpressed(int scancode) {
    return term_keystate[scancode];
}

/*
 * reading from console - interactive mode
 * allows for moving around the screen using cursor keys
 * returns control to the interpreter on pressing enter key
 * puts whole current line in the buffer
 */

void consins_sdl(char *buffer, short nb) {
    term_interactive = 1;
    SDL_Event ev;
    while(term_interactive && SDL_WaitEvent(&ev) >= 0) {
        switch(ev.type) {
        case SDL_QUIT:
            quit();
        case SDL_KEYDOWN:
            if(handle_cursor_keys(ev.key.keysym.scancode)) {
                term_blink = 1;
            } else if (!handle_control_keydown(ev.key.keysym.scancode)) {
                char key = decode_scancode(ev.key.keysym.scancode);
                handle_interactive_mode(key, buffer, nb);
            }
            break;
        case SDL_KEYUP:
            handle_control_keyup(ev.key.keysym.scancode);
            break;
        case SDL_WINDOWEVENT:
            handle_window_event(ev.window);
            break;
        }
        draw();
    }
}


