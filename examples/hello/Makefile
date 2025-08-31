BIN_DIR = ../../build/examples
HELLO := $(BIN_DIR)/hello
SPN := ../../build/bin/spn
CC := gcc

all: $(HELLO)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(HELLO): main.c
	$(SPN) build
	$(CC) main.c $(shell $(SPN) flags include) -o $(HELLO) -lSDL3

.PHONY: all
