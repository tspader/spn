BUILD_DIR = $(shell realpath ../../build)
EXAMPLE_DIR = $(BUILD_DIR)/examples
DRLIBS := $(EXAMPLE_DIR)/dr_libs
SPN := $(BUILD_DIR)/bin/spn
CC := gcc

all: $(DRLIBS)

$(EXAMPLE_DIR):
	@mkdir -p $(EXAMPLE_DIR)

$(DRLIBS): main.c | $(EXAMPLE_DIR)
	$(SPN) build
	$(CC) main.c $(shell $(SPN) flags include) -o $(DRLIBS) $(shell $(SPN) flags lib-include) $(shell $(SPN) flags libs) -lm

.PHONY: all
