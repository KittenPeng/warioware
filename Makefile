CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS =
TARGET = warioware
SRC_DIR = src
BUILD_DIR = build

# Prefer pkg-config; fall back to sdl2-config + SDL2_image + SDL2_ttf + SDL2_mixer
PKG_CONFIG := $(shell command -v pkg-config 2>/dev/null)
ifdef PKG_CONFIG
  SDL_CFLAGS  := $(shell pkg-config --cflags SDL2 SDL2_image SDL2_ttf SDL2_mixer 2>/dev/null)
  SDL_LDFLAGS := $(shell pkg-config --libs SDL2 SDL2_image SDL2_ttf SDL2_mixer 2>/dev/null)
endif
ifndef SDL_CFLAGS
  SDL_CFLAGS  := $(shell sdl2-config --cflags 2>/dev/null) -I/opt/homebrew/include/SDL2 -I/usr/local/include/SDL2
  SDL_LDFLAGS := $(shell sdl2-config --libs 2>/dev/null) -L/opt/homebrew/lib -L/usr/local/lib -lSDL2_image -lSDL2_ttf -lSDL2_mixer
endif

CFLAGS  += $(SDL_CFLAGS)
LDFLAGS += $(SDL_LDFLAGS)

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/elevator.c
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean run watch

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(TARGET)
	./$(TARGET)

# Rebuild every 2 seconds when source changes (no extra tools required)
watch:
	@while true; do make -q $(TARGET) 2>/dev/null || make; sleep 2; done

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
