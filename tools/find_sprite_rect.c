/*
 * One-off tool: load elevator.png and print the bounding box of the first
 * elevator sprite (non-green pixels under the "Next" area).
 * Build: make -f Makefile.find_sprite_rect
 * Run from project root: ./tools/find_sprite_rect
 */
#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>

/* Dark green background in the sheet (with tolerance). */
#define GREEN_R_MIN  20
#define GREEN_R_MAX  35
#define GREEN_G_MIN  70
#define GREEN_G_MAX  90
#define GREEN_B_MIN  40
#define GREEN_B_MAX  60

static int is_green(Uint8 r, Uint8 g, Uint8 b)
{
    return r >= GREEN_R_MIN && r <= GREEN_R_MAX &&
           g >= GREEN_G_MIN && g <= GREEN_G_MAX &&
           b >= GREEN_B_MIN && b <= GREEN_B_MAX;
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "assets/graphics/elevator.png";
    (void)argc;
    (void)argv;

    /* No video needed for image load; avoid SDL_Init(VIDEO) for headless. */
    if (SDL_Init(0) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG) {
        fprintf(stderr, "IMG_Init: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Surface *surf = IMG_Load(path);
    if (!surf) {
        fprintf(stderr, "IMG_Load: %s\n", IMG_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    /* Convert to RGBA for easy pixel read. */
    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);
    if (!rgba) {
        fprintf(stderr, "SDL_ConvertSurfaceFormat: %s\n", SDL_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    int w = rgba->w;
    int h = rgba->h;
    fprintf(stderr, "Image size: %d x %d\n", w, h);

    /* Search region: first frame only (240×160 under "Next" at y=80). */
    int search_y_start = 80;
    int search_y_end   = 80 + 160;
    int search_x_end   = (w > 240) ? 240 : w;

    if (SDL_MUSTLOCK(rgba))
        SDL_LockSurface(rgba);

    int top = -1, left = w, right = -1, bottom = -1;
    Uint8 *p = (Uint8 *)rgba->pixels;
    int pitch = rgba->pitch;

    for (int y = search_y_start; y < search_y_end && y < h; y++) {
        for (int x = 0; x < search_x_end && x < w; x++) {
            Uint8 *px = p + (size_t)y * pitch + (size_t)x * 4;
            Uint8 r = px[0], g = px[1], b = px[2];
            if (!is_green(r, g, b)) {
                if (top < 0) top = y;
                if (y > bottom) bottom = y;
                if (x < left) left = x;
                if (x > right) right = x;
            }
        }
    }

    if (SDL_MUSTLOCK(rgba))
        SDL_UnlockSurface(rgba);

    if (top < 0 || left >= right) {
        fprintf(stderr, "No non-green content found in search region.\n");
        SDL_FreeSurface(rgba);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    int rect_w = right - left + 1;
    int rect_h = bottom - top + 1;
    printf("/* First elevator sprite (non-green) bounding box */\n");
    printf("#define ELEVATOR_SRC_X     %d\n", left);
    printf("#define ELEVATOR_SRC_Y     %d\n", top);
    printf("#define ELEVATOR_SRC_W     %d\n", rect_w);
    printf("#define ELEVATOR_SRC_H     %d\n", rect_h);

    SDL_FreeSurface(rgba);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
