#include "elevator.h"
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>

#define ELEVATOR_FONT_PATH "assets/warioware font.otf/warioware-inc-mega-microgame-big.otf"
#define ELEVATOR_FONT_SIZE 14

/* Exact sprite rects in elevator.png (inclusive coords: top-left to bottom-right).
 * Sprite 1: 4,66 to 243,225   Sprite 2: 254,66 to 493,225   Sprite 3: 504,66 to 743,225 */
#define ELEVATOR_SPRITE_W  240
#define ELEVATOR_SPRITE_H  160
static const SDL_Rect ELEVATOR_SRC_RECTS[3] = {
    {  4, 66, ELEVATOR_SPRITE_W, ELEVATOR_SPRITE_H },
    { 254, 66, ELEVATOR_SPRITE_W, ELEVATOR_SPRITE_H },
    { 504, 66, ELEVATOR_SPRITE_W, ELEVATOR_SPRITE_H },
};

/* "Next" row has 3 frames; cycle through them. */
#define ELEVATOR_NEXT_FRAMES      3
#define ELEVATOR_FRAME_DURATION   0.07f
#define ELEVATOR_OPEN_FRAMES      10
#define ELEVATOR_OPEN_FRAME_TIME  0.08f

/* "Open" animation: 2 rows of 5 sprites. Exact coords (top-left to bottom-right inclusive). */
static const SDL_Rect ELEVATOR_OPEN_RECTS[10] = {
    {   1, 310, ELEVATOR_SPRITE_W, ELEVATOR_SPRITE_H },  /*  1: 1,310 to 240,469 */
    { 251, 311, ELEVATOR_SPRITE_W, ELEVATOR_SPRITE_H },  /*  2: 251,311 to 490,470 */
    { 501, 311, ELEVATOR_SPRITE_W, ELEVATOR_SPRITE_H },  /*  3: 501,311 to 740,470 */
    { 751, 311, ELEVATOR_SPRITE_W, ELEVATOR_SPRITE_H },  /*  4: 751,311 to 990,470 */
    {1001, 311, ELEVATOR_SPRITE_W, ELEVATOR_SPRITE_H },  /*  5: 1001,311 to 1240,470 */
    {   1, 476, ELEVATOR_SPRITE_W, ELEVATOR_SPRITE_H },  /*  6: 1,476 to 240,635 */
    { 251, 476, ELEVATOR_SPRITE_W, ELEVATOR_SPRITE_H },  /*  7: 251,476 to 490,635 */
    { 501, 476, ELEVATOR_SPRITE_W, ELEVATOR_SPRITE_H },  /*  8: 501,476 to 740,635 */
    { 751, 476, ELEVATOR_SPRITE_W, ELEVATOR_SPRITE_H },  /*  9: 751,476 to 990,635 */
    {1001, 476, ELEVATOR_SPRITE_W, ELEVATOR_SPRITE_H },  /* 10: 1001,476 to 1240,635 */
};

typedef enum { ELEVATOR_STATE_IDLE, ELEVATOR_STATE_DOORS_OPENING, ELEVATOR_STATE_MINIGAME, ELEVATOR_STATE_DOORS_CLOSING } ElevatorState;

struct ElevatorScene {
    SDL_Renderer *renderer;
    SDL_Texture  *sprite_sheet;
    SDL_Texture  *mug_shot_sheet;
    SDL_Texture  *bomb_timer_sheet;
    TTF_Font     *font;
    int           sheet_w;
    int           sheet_h;
    int           window_w;
    int           window_h;
    int           current_floor;
    int           lives;
    ElevatorState state;
    int           anim_frame;
    float         anim_timer;
    float         minigame_timer;  /* countdown 4→0, then return to elevator */
};

/* Green background in the elevator sheet (chroma key). */
#define ELEVATOR_CHROMA_R  25
#define ELEVATOR_CHROMA_G  128
#define ELEVATOR_CHROMA_B  93

/* Green background in mug shot sheet (chroma key). */
#define MUG_SHOT_CHROMA_R  24
#define MUG_SHOT_CHROMA_G  126
#define MUG_SHOT_CHROMA_B  55

/* Mug shot overlay sprite: 2,2 to 241,161 (240×160) */
#define MUG_SHOT_SRC_X  2
#define MUG_SHOT_SRC_Y  2
#define MUG_SHOT_SRC_W  240
#define MUG_SHOT_SRC_H  160

/* Bomb timer: 4 seconds, 4 frames (rope long → short → explosion). Sheet 240×129, 4 frames in a row. */
#define BOMB_TIMER_DURATION    4.0f
#define BOMB_TIMER_FRAMES      4
#define BOMB_TIMER_FRAME_W     60
#define BOMB_TIMER_FRAME_H     129
#define BOMB_TIMER_CHROMA_R    0x88
#define BOMB_TIMER_CHROMA_G    0x88
#define BOMB_TIMER_CHROMA_B    0x88

static SDL_Texture *load_texture_with_chroma(SDL_Renderer *renderer, const char *path, Uint8 r, Uint8 g, Uint8 b)
{
    SDL_Surface *surf = IMG_Load(path);
    if (!surf) {
        fprintf(stderr, "Failed to load '%s': %s\n", path, IMG_GetError());
        return NULL;
    }
    Uint32 key = SDL_MapRGB(surf->format, r, g, b);
    if (SDL_SetColorKey(surf, SDL_TRUE, key) != 0) {
        fprintf(stderr, "SDL_SetColorKey: %s\n", SDL_GetError());
        SDL_FreeSurface(surf);
        return NULL;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex)
        fprintf(stderr, "SDL_CreateTextureFromSurface: %s\n", SDL_GetError());
    return tex;
}

static SDL_Texture *load_elevator_texture(SDL_Renderer *renderer, const char *path)
{
    return load_texture_with_chroma(renderer, path, ELEVATOR_CHROMA_R, ELEVATOR_CHROMA_G, ELEVATOR_CHROMA_B);
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

    scene->mug_shot_sheet = load_texture_with_chroma(renderer, "assets/graphics/mug shot.png",
                                                      MUG_SHOT_CHROMA_R, MUG_SHOT_CHROMA_G, MUG_SHOT_CHROMA_B);
    if (!scene->mug_shot_sheet)
        fprintf(stderr, "Failed to load mug shot sheet\n");

    scene->bomb_timer_sheet = load_texture_with_chroma(renderer, "assets/graphics/bomb timer.png",
                                                        BOMB_TIMER_CHROMA_R, BOMB_TIMER_CHROMA_G, BOMB_TIMER_CHROMA_B);
    if (!scene->bomb_timer_sheet)
        fprintf(stderr, "Failed to load bomb timer sheet\n");

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
    if (scene->mug_shot_sheet)
        SDL_DestroyTexture(scene->mug_shot_sheet);
    if (scene->bomb_timer_sheet)
        SDL_DestroyTexture(scene->bomb_timer_sheet);
    if (scene->sprite_sheet)
        SDL_DestroyTexture(scene->sprite_sheet);
    free(scene);
}

void elevator_scene_update(ElevatorScene *scene, float delta_s)
{
    if (!scene)
        return;

    if (scene->state == ELEVATOR_STATE_MINIGAME) {
        scene->minigame_timer -= delta_s;
        if (scene->minigame_timer <= 0.0f) {
            scene->current_floor++;
            scene->state = ELEVATOR_STATE_DOORS_CLOSING;
            scene->anim_frame = ELEVATOR_OPEN_FRAMES - 1;
            scene->anim_timer = 0.0f;
        }
        return;
    }

    /* DOORS_CLOSING: play door frames in reverse (9→0), then return to idle */
    if (scene->state == ELEVATOR_STATE_DOORS_CLOSING) {
        scene->anim_timer += delta_s;
        while (scene->anim_timer >= ELEVATOR_OPEN_FRAME_TIME) {
            scene->anim_timer -= ELEVATOR_OPEN_FRAME_TIME;
            scene->anim_frame--;
            if (scene->anim_frame < 0) {
                scene->anim_frame = 0;
                scene->state = ELEVATOR_STATE_IDLE;
                break;
            }
        }
        return;
    }

    if (scene->state == ELEVATOR_STATE_IDLE) {
        scene->anim_timer += delta_s;
        while (scene->anim_timer >= ELEVATOR_FRAME_DURATION) {
            scene->anim_timer -= ELEVATOR_FRAME_DURATION;
            scene->anim_frame = (scene->anim_frame + 1) % ELEVATOR_NEXT_FRAMES;
        }
        return;
    }

    /* DOORS_OPENING: advance through 10 frames, then sit on minigame (don't return to idle) */
    scene->anim_timer += delta_s;
    while (scene->anim_timer >= ELEVATOR_OPEN_FRAME_TIME) {
        scene->anim_timer -= ELEVATOR_OPEN_FRAME_TIME;
        scene->anim_frame++;
        if (scene->anim_frame >= ELEVATOR_OPEN_FRAMES) {
            scene->anim_frame = ELEVATOR_OPEN_FRAMES - 1;
            scene->state = ELEVATOR_STATE_MINIGAME;
            scene->minigame_timer = BOMB_TIMER_DURATION;
            break;
        }
    }
}

void elevator_scene_draw(ElevatorScene *scene)
{
    if (!scene || !scene->sprite_sheet)
        return;

    SDL_Renderer *r = scene->renderer;
    int win_w = scene->window_w;
    int win_h = scene->window_h;
    if (win_w <= 0) win_w = 1;
    if (win_h <= 0) win_h = 1;

    SDL_Rect dst = { .x = 0, .y = 0, .w = win_w, .h = win_h };

    if (scene->state == ELEVATOR_STATE_DOORS_OPENING || scene->state == ELEVATOR_STATE_MINIGAME || scene->state == ELEVATOR_STATE_DOORS_CLOSING) {
        /* Black background + mug shot overlay (visible during doors opening, minigame, and doors closing). */
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderFillRect(r, &dst);
        if (scene->mug_shot_sheet) {
            SDL_Rect mug_src = { MUG_SHOT_SRC_X, MUG_SHOT_SRC_Y, MUG_SHOT_SRC_W, MUG_SHOT_SRC_H };
            SDL_RenderCopy(r, scene->mug_shot_sheet, &mug_src, &dst);
        }
        /* Doors opening or closing: draw door sprite (opening = frame 0→9, closing = frame 9→0). */
        if (scene->state == ELEVATOR_STATE_DOORS_OPENING || scene->state == ELEVATOR_STATE_DOORS_CLOSING) {
            int frame = scene->anim_frame;
            if (frame < 0) frame = 0;
            if (frame >= ELEVATOR_OPEN_FRAMES) frame = ELEVATOR_OPEN_FRAMES - 1;
            SDL_Rect src = ELEVATOR_OPEN_RECTS[frame];
            SDL_RenderCopy(r, scene->sprite_sheet, &src, &dst);
        }
        /* During minigame: draw bomb timer (rope shortens over 4s, then explosion). */
        if (scene->state == ELEVATOR_STATE_MINIGAME && scene->bomb_timer_sheet && scene->minigame_timer > 0.0f) {
            float t = BOMB_TIMER_DURATION - scene->minigame_timer;
            int frame = (int)(t / BOMB_TIMER_DURATION * (float)BOMB_TIMER_FRAMES);
            if (frame >= BOMB_TIMER_FRAMES) frame = BOMB_TIMER_FRAMES - 1;
            SDL_Rect bomb_src = {
                .x = frame * BOMB_TIMER_FRAME_W,
                .y = 0,
                .w = BOMB_TIMER_FRAME_W,
                .h = BOMB_TIMER_FRAME_H
            };
            /* Draw bomb timer in top-right corner, scaled to fit. */
            SDL_Rect bomb_dst = {
                .x = win_w - BOMB_TIMER_FRAME_W,
                .y = 0,
                .w = BOMB_TIMER_FRAME_W,
                .h = BOMB_TIMER_FRAME_H
            };
            SDL_RenderCopy(r, scene->bomb_timer_sheet, &bomb_src, &bomb_dst);
        }
    } else {
        /* Idle: just the elevator sprite. */
        int frame = scene->anim_frame;
        if (frame < 0) frame = 0;
        if (frame >= ELEVATOR_NEXT_FRAMES) frame = ELEVATOR_NEXT_FRAMES - 1;
        SDL_Rect src = ELEVATOR_SRC_RECTS[frame];
        SDL_RenderCopy(r, scene->sprite_sheet, &src, &dst);
    }

    /* Floor and lives only visible in elevator idle. */
    if (scene->font && scene->state == ELEVATOR_STATE_IDLE) {
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
    if (event->type == SDL_QUIT)
        return false;
    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_ESCAPE)
            return false;
        /* Spacebar: start doors-opening animation only from idle. */
        if (event->key.keysym.sym == SDLK_SPACE && scene->state == ELEVATOR_STATE_IDLE) {
            scene->state = ELEVATOR_STATE_DOORS_OPENING;
            scene->anim_frame = 0;
            scene->anim_timer = 0.0f;
        }
    }
    return true;
}
