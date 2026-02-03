#include "elevator.h"
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>

#define ELEVATOR_FONT_PATH "assets/warioware font.otf/warioware-inc-mega-microgame-big.otf"
#define ELEVATOR_FONT_SIZE 14

/* First elevator sprite rect (from tools/find_sprite_rect on elevator.png). */
#define ELEVATOR_SRC_X  0
#define ELEVATOR_SRC_Y  80
#define ELEVATOR_SRC_W  240
#define ELEVATOR_SRC_H  160

/* Inset to crop green; right and bottom same for all frames. */
#define ELEVATOR_INSET_RIGHT   2
#define ELEVATOR_INSET_BOTTOM  10
/* Margin so drawing stays inside viewport (avoids top/right cut-off). */
#define ELEVATOR_DRAW_MARGIN   4
/* Per-frame left inset: frame 1 has green on left, frame 2 bleeds from sprite 2. */
#define ELEVATOR_INSET_LEFT_F0  6
#define ELEVATOR_INSET_LEFT_F1 14
#define ELEVATOR_INSET_LEFT_F2 28   /* sprite 3: further right to avoid sprite 2 */

/* "Next" row has 3 frames; cycle through them. */
#define ELEVATOR_NEXT_FRAMES    3
#define ELEVATOR_FRAME_DURATION 0.07f

struct ElevatorScene {
    SDL_Renderer *renderer;
    SDL_Texture  *sprite_sheet;
    TTF_Font     *font;
    int           sheet_w;
    int           sheet_h;
    int           window_w;
    int           window_h;
    /* Game state (for later: minigames, floors) */
    int           current_floor;
    int           lives;
    /* Idle animation: 0, 1, 2 then loop. */
    int           anim_frame;
    float         anim_timer;
};

static SDL_Texture *load_elevator_texture(SDL_Renderer *renderer, const char *path)
{
    SDL_Texture *tex = IMG_LoadTexture(renderer, path);
    if (!tex)
        fprintf(stderr, "Failed to load '%s': %s\n", path, IMG_GetError());
    return tex;
}

/* Render text at (x, y); caller does not own the returned texture (short-lived). */
static void draw_text(SDL_Renderer *rend, TTF_Font *font, const char *text, int x, int y, Uint8 red, Uint8 grn, Uint8 blu)
{
    if (!font || !text)
        return;
    SDL_Color color = { red, grn, blu, 255 };
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
    if (!surf)
        return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
    SDL_FreeSurface(surf);
    if (!tex)
        return;
    int w, h;
    SDL_QueryTexture(tex, NULL, NULL, &w, &h);
    SDL_Rect dst = { .x = x, .y = y, .w = w, .h = h };
    SDL_RenderCopy(rend, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

/* Render text centered at (cx, cy). */
static void draw_text_centered(SDL_Renderer *rend, TTF_Font *font, const char *text, int cx, int cy, Uint8 red, Uint8 grn, Uint8 blu)
{
    if (!font || !text)
        return;
    SDL_Color color = { red, grn, blu, 255 };
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
    if (!surf)
        return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
    SDL_FreeSurface(surf);
    if (!tex)
        return;
    int w, h;
    SDL_QueryTexture(tex, NULL, NULL, &w, &h);
    SDL_Rect dst = { .x = cx - w / 2, .y = cy - h / 2, .w = w, .h = h };
    SDL_RenderCopy(rend, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

ElevatorScene *elevator_scene_create(SDL_Renderer *renderer)
{
    ElevatorScene *scene = calloc(1, sizeof(ElevatorScene));
    if (!scene)
        return NULL;

    scene->renderer   = renderer;
    scene->lives      = 4;
    scene->current_floor = 1;

    scene->sprite_sheet = load_elevator_texture(renderer, "assets/graphics/elevator.png");
    if (!scene->sprite_sheet) {
        elevator_scene_destroy(scene);
        return NULL;
    }

    scene->font = TTF_OpenFont(ELEVATOR_FONT_PATH, ELEVATOR_FONT_SIZE);
    if (!scene->font)
        fprintf(stderr, "TTF_OpenFont '%s': %s\n", ELEVATOR_FONT_PATH, TTF_GetError());

    if (SDL_QueryTexture(scene->sprite_sheet, NULL, NULL, &scene->sheet_w, &scene->sheet_h) != 0) {
        fprintf(stderr, "SDL_QueryTexture: %s\n", SDL_GetError());
        elevator_scene_destroy(scene);
        return NULL;
    }

    return scene;
}

void elevator_scene_destroy(ElevatorScene *scene)
{
    if (!scene)
        return;
    if (scene->font)
        TTF_CloseFont(scene->font);
    if (scene->sprite_sheet)
        SDL_DestroyTexture(scene->sprite_sheet);
    free(scene);
}

void elevator_scene_update(ElevatorScene *scene, float delta_s)
{
    if (!scene)
        return;
    scene->anim_timer += delta_s;
    while (scene->anim_timer >= ELEVATOR_FRAME_DURATION) {
        scene->anim_timer -= ELEVATOR_FRAME_DURATION;
        scene->anim_frame = (scene->anim_frame + 1) % ELEVATOR_NEXT_FRAMES;
    }
}

void elevator_scene_draw(ElevatorScene *scene)
{
    if (!scene || !scene->sprite_sheet)
        return;

    SDL_Renderer *r = scene->renderer;

    /* Current animation frame (0, 1, or 2); per-frame left inset to hide green and avoid bleed. */
    int frame = scene->anim_frame;
    if (frame < 0) frame = 0;
    if (frame >= ELEVATOR_NEXT_FRAMES) frame = ELEVATOR_NEXT_FRAMES - 1;
    int inset_left = (frame == 0) ? ELEVATOR_INSET_LEFT_F0 : (frame == 1) ? ELEVATOR_INSET_LEFT_F1 : ELEVATOR_INSET_LEFT_F2;
    SDL_Rect src = {
        .x = ELEVATOR_SRC_X + (frame * ELEVATOR_SRC_W) + inset_left,
        .y = ELEVATOR_SRC_Y,
        .w = ELEVATOR_SRC_W - inset_left - ELEVATOR_INSET_RIGHT,
        .h = ELEVATOR_SRC_H - ELEVATOR_INSET_BOTTOM
    };

    int win_w = scene->window_w;
    int win_h = scene->window_h;
    if (win_w <= 0) win_w = 1;
    if (win_h <= 0) win_h = 1;

    /* Draw with margin inside viewport so top/right aren't clipped. */
    int m = ELEVATOR_DRAW_MARGIN;
    SDL_Rect dst = {
        .x = m,
        .y = m,
        .w = win_w - 2 * m,
        .h = win_h - 2 * m
    };

    SDL_RenderCopy(r, scene->sprite_sheet, &src, &dst);

    /* Draw floor and lives with custom font (white). */
    if (scene->font) {
        char buf[32];
        (void)snprintf(buf, sizeof buf, "Floor %d", scene->current_floor);
        draw_text(r, scene->font, buf, 8, 8, 255, 255, 255);
        (void)snprintf(buf, sizeof buf, "Lives: %d", scene->lives);
        draw_text(r, scene->font, buf, 8, 24, 255, 255, 255);
    }
}

void elevator_scene_set_window_size(ElevatorScene *scene, int w, int h)
{
    if (scene) {
        scene->window_w = w;
        scene->window_h = h;
    }
}

bool elevator_scene_process_event(ElevatorScene *scene, const SDL_Event *event)
{
    (void)scene;
    if (event->type == SDL_QUIT)
        return false;
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE)
        return false;
    return true;
}
