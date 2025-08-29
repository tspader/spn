BIN_DIR = ../../build/examples
KITCHEN_SINK := $(BIN_DIR)/kitchen_sink
SPN := ../../build/bin/spn
CC := gcc

all: $(KITCHEN_SINK)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(KITCHEN_SINK): main.c
	$(SPN) build
	$(CC) main.c $(shell $(SPN) flags include) -o $(KITCHEN_SINK) -lSDL3

.PHONY: all
