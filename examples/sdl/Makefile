BIN_DIR = $(shell realpath ../../build/examples)
SDL := $(BIN_DIR)/sdl
SPN := ../../build/bin/spn
CC := gcc

all: $(SDL)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(SDL): main.c
	$(SPN) build
	$(CC) main.c $(shell $(SPN) flags include) -o $(SDL) $(shell $(SPN) flags lib-include) $(shell $(SPN) flags libs)

.PHONY: all
