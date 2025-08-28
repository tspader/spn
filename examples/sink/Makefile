SPN_DIR = ../..
BUILD_DIR = $(SPN_DIR)/build/examples
SINK := $(BUILD_DIR)/sink

SPN := $(SPN_DIR)/build/bin/spn
CC := gcc

EX_SINK_FLAG_LANGUAGE := -std=c11
EX_SINK_FLAG_OPTIMIZATION := -g
EX_SINK_FLAG_OUTPUT := -o $(SINK)
EX_SINK_FLAG_INCLUDE := \
	-I$(shell $(SPN) flags include --package sdl3) \
  -I$(shell $(SPN) flags include --package sp) \
  -I$(shell $(SPN) flags include --package argparse)
EX_SINK_FLAG_LIBS := \
	$(shell $(SPN) flags libs --package sdl3)/libSDL3.so
EX_SINK_CC_FLAGS := $(EX_SINK_FLAG_LANGUAGE) $(EX_SINK_FLAG_OPTIMIZATION) $(EX_SINK_FLAG_INCLUDE) $(EX_SINK_FLAG_OUTPUT)

all: $(SINK)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(SINK): main.c
	$(SPN) build
	$(CC) main.c $(EX_SINK_CC_FLAGS) $(EX_SINK_FLAG_LIBS)

.PHONY: all
