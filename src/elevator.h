#ifndef ELEVATOR_H
#define ELEVATOR_H

#include <SDL.h>
#include <stdbool.h>

typedef struct ElevatorScene ElevatorScene;

/* Creates the elevator scene (loads sprites, etc.). Returns NULL on failure. */
ElevatorScene *elevator_scene_create(SDL_Renderer *renderer);

/* Frees the elevator scene. */
void elevator_scene_destroy(ElevatorScene *scene);

/* Updates elevator state (animations, etc.). Delta in seconds. */
void elevator_scene_update(ElevatorScene *scene, float delta_s);

/* Draws the elevator scene to the current render target. */
void elevator_scene_draw(ElevatorScene *scene);

/* Sets the window size for correct scaling. */
void elevator_scene_set_window_size(ElevatorScene *scene, int w, int h);

/* Returns false when the game should quit. */
bool elevator_scene_process_event(ElevatorScene *scene, const SDL_Event *event);

#endif /* ELEVATOR_H */
