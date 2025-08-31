BUILD_DIR = $(shell realpath ../../build)
EXAMPLE_DIR = $(BUILD_DIR)/examples
HELLO := $(EXAMPLE_DIR)/hello
SPN := $(BUILD_DIR)/bin/spn
CC := gcc

.PHONY: all
all: $(HELLO)

$(EXAMPLE_DIR):
	@mkdir -p $(EXAMPLE_DIR)

$(HELLO): main.c | $(EXAMPLE_DIR)
	$(SPN) build
	$(CC) main.c $(shell $(SPN) flags include) -o $(HELLO) $(shell $(SPN) flags lib-include) $(shell $(SPN) flags libs) -lm
