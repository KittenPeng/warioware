#include "elevator.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>

#define WINDOW_TITLE   "WarioWare"
#define WINDOW_WIDTH   240
#define WINDOW_HEIGHT  160

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;

static int init_sdl(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return -1;
    }
    int img_flags = IMG_INIT_PNG;
    if ((IMG_Init(img_flags) & img_flags) != img_flags) {
        fprintf(stderr, "IMG_Init: %s\n", IMG_GetError());
        SDL_Quit();
        return -1;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        IMG_Quit();
        SDL_Quit();
        return -1;
    }
    return 0;
}

static void quit_sdl(void)
{
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

static int create_window(void)
{
    g_window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return -1;
    }
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        return -1;
    }
    /* Fixed 240×160 logical size so content is never cut off (scales to window). */
    SDL_RenderSetLogicalSize(g_renderer, WINDOW_WIDTH, WINDOW_HEIGHT);
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (init_sdl() != 0)
        return EXIT_FAILURE;

    if (create_window() != 0) {
        quit_sdl();
        return EXIT_FAILURE;
    }

    ElevatorScene *elevator = elevator_scene_create(g_renderer);
    if (!elevator) {
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        quit_sdl();
        return EXIT_FAILURE;
    }

    elevator_scene_set_window_size(elevator, WINDOW_WIDTH, WINDOW_HEIGHT);

    int running = 1;
    Uint64 last_ticks = SDL_GetPerformanceCounter();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (!elevator_scene_process_event(elevator, &event))
                running = 0;
        }

        Uint64 now = SDL_GetPerformanceCounter();
        float delta = (float)(now - last_ticks) / (float)SDL_GetPerformanceFrequency();
        last_ticks = now;

        elevator_scene_update(elevator, delta);

        /* Use logical size (always 240×160) so drawing coordinates are correct. */
        int w, h;
        SDL_RenderGetLogicalSize(g_renderer, &w, &h);
        if (w <= 0) w = WINDOW_WIDTH;
        if (h <= 0) h = WINDOW_HEIGHT;
        elevator_scene_set_window_size(elevator, w, h);

        SDL_SetRenderDrawColor(g_renderer, 0x1a, 0x4d, 0x2e, 255); /* dark green background */
        SDL_RenderClear(g_renderer);
        elevator_scene_draw(elevator);
        SDL_RenderPresent(g_renderer);
    }

    elevator_scene_destroy(elevator);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    quit_sdl();
    return EXIT_SUCCESS;
}
