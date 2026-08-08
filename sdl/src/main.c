// A real SDL2 front end for the Game Boy core (src/) - Phase 7's "real
// graphical front end" (see docs/GAMEBOY_ROADMAP.md), replacing the
// original GTK4+Cairo+CoreAudio front end this project shipped first.
// SDL2 is the better fit for a pixel-framebuffer emulator specifically:
// SDL_Renderer/SDL_Texture is built around blitting raw pixel buffers
// (what draw_frame() below does) rather than a widget-toolkit drawing
// model, and SDL_QueueAudio gives the same "push samples, the device
// plays them" model the old CoreAudio AudioQueue code used, but
// portably - so this rewrite also drops the macOS-only AudioToolbox
// dependency the GTK version needed. Like that version, the core is
// linked directly into one binary rather than spawning a separate
// process: the Game Boy's output is a pixel framebuffer, not a text/
// escape-code stream, so there's no terminal-widget-shaped reason to
// spawn a child process the way the sibling Z80/CP-M repo's cpm/gtk
// does.
//
// Kept as its own opt-in binary (`make gameboy-sdl`), same reasoning as
// the GTK version it replaces: the SDL2 dependency stays out of the
// default plain `make` build, and src/main.c's existing
// --ppm/--wav/--input bring-up driver keeps working unmodified for
// testing.
//
// Real-time video, keyboard input, live audio, and save states (F5
// save / F9 load, src/savestate.c - see handle_key_down() below). CGB
// support is still unimplemented anywhere in this project - see
// docs/GAMEBOY_ROADMAP.md's Phase 7.

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "mmu.h"
#include "cart.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"
#include "apu.h"
#include "savestate.h"

// Integer upscale factor - nearest-neighbor (see main()'s
// SDL_HINT_RENDER_SCALE_QUALITY below), so the real 160x144 pixel grid
// stays sharp rather than blurring into a smooth 800x720ish window.
#define SCALE 4

#define AUDIO_SAMPLE_RATE 44100
// Interleaved-stereo floats (gb_apu_step()'s own output format) - one
// real video frame produces roughly 738 stereo pairs at 44.1kHz (70224
// T-states/frame / (4194304/44100) T-states/sample), so 4096 is
// comfortable headroom above the ~1476 floats/frame this actually needs,
// not a size chosen to matter on its own (see flush_audio()'s comment
// for how this buffer gets drained every tick regardless of how full it
// is).
#define AUDIO_BUFFER_CAP 4096

// ~59.7 Hz is the real DMG frame rate (pandocs' Rendering.md: 154
// scanlines * 456 dots / 4.194304 MHz), but SDL_Delay() only has
// millisecond granularity - 16ms is the closest round number (62.5 Hz,
// ~4.7% fast). Not noticeable during actual play, and simpler than a
// higher-resolution timer source for this first pass - the same
// documented approximation the GTK front end's g_timeout_add(16, ...)
// used.
#define FRAME_INTERVAL_MS 16

typedef struct {
    GBCpu cpu;
    GBCart cart;
    GBPpu ppu;
    GBTimer timer;
    GBJoypad joypad;
    GBApu apu;
    float *audio_buffer; // apu's sample_buffer - see flush_audio()
    SDL_AudioDeviceID audio_dev; // 0 if SDL_OpenAudioDevice() failed - video/input still work
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    uint32_t pixel_buffer[GB_SCREEN_HEIGHT][GB_SCREEN_WIDTH]; // ARGB8888, refilled each draw_frame()
    char *save_state_path; // "<rom>.state" - see handle_key_down()'s F5/F9 handling below
} GameboyApp;

// DMG shade index (0=white..3=black) -> 8-bit grayscale sample, exactly
// matching src/main.c's own shade_to_gray() - keeps this front end's
// look consistent with the --ppm dumps used for testing rather than
// inventing a separate (unconfirmed) "real DMG green" palette.
static uint8_t shade_to_gray(uint8_t shade) {
    switch (shade) {
        case 0: return 255;
        case 1: return 170;
        case 2: return 85;
        default: return 0; // 3
    }
}

static void draw_frame(GameboyApp *app) {
    for (int y = 0; y < GB_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < GB_SCREEN_WIDTH; x++) {
            uint8_t g = shade_to_gray(app->ppu.framebuffer[y][x]);
            app->pixel_buffer[y][x] = (uint32_t)0xFF000000 | ((uint32_t)g << 16) | ((uint32_t)g << 8) | g;
        }
    }
    SDL_UpdateTexture(app->texture, NULL, app->pixel_buffer, GB_SCREEN_WIDTH * (int)sizeof(uint32_t));
    SDL_RenderClear(app->renderer);
    SDL_RenderCopy(app->renderer, app->texture, NULL, NULL);
    SDL_RenderPresent(app->renderer);
}

// SDL_QueueAudio() is a "push" API - gb_apu_step() already produces
// samples on its own schedule (paced by the same ~16ms frame loop
// driving video, called once per main-loop iteration) rather than the
// audio device pulling samples via a callback, so this fits directly
// with no ring-buffer/lock-free bookkeeping, same reasoning the old
// CoreAudio AudioQueue version used. Hands whatever gb_apu_step() has
// appended to apu.sample_buffer since the last tick to the device, then
// resets the append position.
static void flush_audio(GameboyApp *app) {
    if (!app->audio_dev || app->apu.sample_buffer_len <= 0) return;
    Uint32 byte_size = (Uint32)(app->apu.sample_buffer_len * sizeof(float));
    SDL_QueueAudio(app->audio_dev, app->audio_buffer, byte_size);
    app->apu.sample_buffer_len = 0;
}

static void setup_audio(GameboyApp *app) {
    SDL_AudioSpec desired;
    SDL_zero(desired);
    desired.freq = AUDIO_SAMPLE_RATE;
    desired.format = AUDIO_F32SYS;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = NULL; // NULL callback puts the device in SDL_QueueAudio() "push" mode

    SDL_AudioSpec obtained;
    app->audio_dev = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (!app->audio_dev) {
        fprintf(stderr, "SDL: SDL_OpenAudioDevice failed (%s) - continuing without audio\n", SDL_GetError());
        return;
    }
    SDL_PauseAudioDevice(app->audio_dev, 0);
}

// Runs the core until one real video frame completes (VBlank), the same
// per-tick shape src/main.c's own --ppm loop uses. Returns 0 on an
// illegal/unimplemented opcode (caller should stop), 1 otherwise.
static int step_frame(GameboyApp *app) {
    // One frame's worth of instructions is a few thousand at most,
    // generously bounded here just so a genuinely stuck core (e.g. an
    // infinite loop with interrupts disabled) can't hang the main loop
    // indefinitely.
    for (int guard = 0; guard < 500000; guard++) {
        int cycles = gb_cpu_step(&app->cpu);
        if (cycles < 0) {
            fprintf(stderr, "Illegal/unimplemented opcode at PC=0x%04X - stopping\n",
                    (unsigned)(app->cpu.pc - 1));
            return 0;
        }
        gb_ppu_step(&app->ppu, &app->cpu, cycles);
        gb_timer_step(&app->timer, &app->cpu, cycles);
        gb_apu_step(&app->apu, &app->cpu, cycles);
        if (app->ppu.frame_ready) {
            app->ppu.frame_ready = 0;
            break;
        }
    }
    return 1;
}

// Arrows = D-pad, Z = B, X = A, Enter = Start, Right Shift = Select -
// the same layout convention several existing Game Boy emulators
// (BGB, SameBoy) default to, not invented here. Returns -1 (via
// *button) for any unmapped key, which the caller passes through
// unhandled so normal window shortcuts still work. SDL keycodes are
// already case/shift-normalized (SDLK_z covers both Z and z), unlike
// GDK's separate upper/lowercase keyvals the GTK version had to list.
static int key_to_button(SDL_Keycode key, int *is_direction) {
    switch (key) {
        case SDLK_UP:    *is_direction = 1; return GB_BUTTON_UP;
        case SDLK_DOWN:  *is_direction = 1; return GB_BUTTON_DOWN;
        case SDLK_LEFT:  *is_direction = 1; return GB_BUTTON_LEFT;
        case SDLK_RIGHT: *is_direction = 1; return GB_BUTTON_RIGHT;
        case SDLK_z: *is_direction = 0; return GB_BUTTON_B;
        case SDLK_x: *is_direction = 0; return GB_BUTTON_A;
        case SDLK_RETURN: case SDLK_KP_ENTER: *is_direction = 0; return GB_BUTTON_START;
        case SDLK_RSHIFT: *is_direction = 0; return GB_BUTTON_SELECT;
        default: return -1;
    }
}

static void handle_key(GameboyApp *app, SDL_Keycode key, int pressed) {
    int is_direction;
    int button = key_to_button(key, &is_direction);
    if (button < 0) return;
    if (is_direction) gb_joypad_set_direction(&app->joypad, &app->cpu, button, pressed);
    else gb_joypad_set_action(&app->joypad, &app->cpu, button, pressed);
}

// F5/F9 - the same save/load-state key convention several existing
// emulators (VBA-M, Dolphin, RetroArch's default bindings) use, not
// invented here. Checked on key-down only, and `repeat` is used to
// ignore OS key-repeat so holding F5 doesn't spam-save.
static void handle_key_down(GameboyApp *app, SDL_Keycode key, int repeat) {
    if (repeat) return;
    if (key == SDLK_F5) {
        gb_savestate_save(&app->cpu, app->save_state_path);
        return;
    }
    if (key == SDLK_F9) {
        gb_savestate_load(&app->cpu, app->save_state_path);
        return;
    }
    handle_key(app, key, 1);
}

int main(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        printf("Usage:\n  %s <rom.gb>   Run a Game Boy ROM in a real SDL2 window\n\n", argv[0]);
        printf("Keys: arrows = D-pad, Z = B, X = A, Enter = Start, Right Shift = Select,\n"
               "      F5 = save state, F9 = load state (to/from '<rom>.state')\n");
        return argc < 2 ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    char *rom_path = argv[1];

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    GameboyApp app;
    memset(&app, 0, sizeof(app));

    if (gb_cart_load(&app.cart, rom_path) != 0) {
        // gb_cart_load() already printed why - nothing more useful to
        // do than give up (matches src/main.c's own behavior).
        SDL_Quit();
        return EXIT_FAILURE;
    }

    app.cpu.memory = calloc(1, GB_MEM_SIZE);
    app.cpu.cart = &app.cart;
    app.cpu.ppu = &app.ppu;
    app.cpu.timer = &app.timer;
    app.cpu.joypad = &app.joypad;
    app.cpu.apu = &app.apu;

    gb_ppu_reset(&app.ppu);
    gb_timer_reset(&app.timer);
    gb_joypad_reset(&app.joypad);
    app.audio_buffer = calloc(AUDIO_BUFFER_CAP, sizeof(float));
    gb_apu_reset(&app.apu, AUDIO_SAMPLE_RATE, app.audio_buffer, AUDIO_BUFFER_CAP);
    setup_audio(&app);
    gb_cpu_init_tables();
    gb_cpu_reset(&app.cpu);

    // "<rom path>.state" - simple and predictable rather than a
    // separate save-slot picker UI, which nothing about this front end
    // has needed yet (same "start simple" reasoning key_to_button()'s
    // fixed layout already applies).
    size_t path_len = strlen(rom_path);
    app.save_state_path = malloc(path_len + 7); // ".state\0"
    memcpy(app.save_state_path, rom_path, path_len);
    strcpy(app.save_state_path + path_len, ".state");

    app.window = SDL_CreateWindow(rom_path, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   GB_SCREEN_WIDTH * SCALE, GB_SCREEN_HEIGHT * SCALE, SDL_WINDOW_SHOWN);
    if (!app.window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    // "0" = nearest-neighbor, so the real 160x144 grid stays sharp when
    // stretched up to the window's SCALE-times size below (see this
    // file's own top-of-file SCALE comment).
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED);
    if (!app.renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    app.texture = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                     GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    if (!app.texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                case SDL_KEYDOWN:
                    handle_key_down(&app, event.key.keysym.sym, event.key.repeat);
                    break;
                case SDL_KEYUP:
                    handle_key(&app, event.key.keysym.sym, 0);
                    break;
                default:
                    break;
            }
        }

        Uint32 tick_start = SDL_GetTicks();
        if (!step_frame(&app)) {
            running = 0;
            break;
        }
        draw_frame(&app);
        flush_audio(&app);

        Uint32 elapsed = SDL_GetTicks() - tick_start;
        if (elapsed < FRAME_INTERVAL_MS) SDL_Delay(FRAME_INTERVAL_MS - elapsed);
    }

    if (app.audio_dev) SDL_CloseAudioDevice(app.audio_dev);
    if (app.texture) SDL_DestroyTexture(app.texture);
    if (app.renderer) SDL_DestroyRenderer(app.renderer);
    if (app.window) SDL_DestroyWindow(app.window);
    SDL_Quit();

    free(app.cpu.memory);
    free(app.audio_buffer);
    free(app.save_state_path);
    gb_cart_free(&app.cart);
    return EXIT_SUCCESS;
}
