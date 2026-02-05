# warioware

A minigame elevator game: clear minigames on each floor and try to survive with 4 lives.

## Requirements

- GCC (or compatible C compiler)
- Make
- [SDL2](https://www.libsdl.org/), [SDL2_image](https://github.com/libsdl-org/SDL_image), [SDL2_ttf](https://github.com/libsdl-org/SDL_ttf), and [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer)

**macOS (Homebrew):**
```sh
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

**Linux:** Install `libsdl2-dev`, `libsdl2-image-dev`, `libsdl2-ttf-dev`, and `libsdl2-mixer-dev` (e.g. `apt install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev`).

## Build

```sh
make
```

## Auto-rebuild on change

Run `make watch` in a terminal; the game will rebuild every 2 seconds when you change source files. Press Ctrl+C to stop.

## Run

Run from the project root so the game finds `assets/graphics/elevator.png`:

```sh
make run
```
or
```sh
./warioware
```

## Clean

```sh
make clean
```
