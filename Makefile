ROOT        := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
HOST_TRIPLE := $(shell uname -m)-linux-gnu
TRIPLE      ?= $(HOST_TRIPLE)

BUILD := $(ROOT)/.build
WORK  := $(BUILD)/work/$(TRIPLE)
STORE := $(BUILD)/store/$(TRIPLE)
BIN   := $(STORE)/bin/spn

.PHONY: all build configure fetch test install uninstall clean
all: build
ifeq ($(TRIPLE),$(HOST_TRIPLE))
	@ln -sfn .build/store/$(TRIPLE) $(ROOT)/bootstrap
	@ln -sf .build/work/$(TRIPLE)/compile_commands.json $(ROOT)/compile_commands.json
	@echo "host binary: bootstrap/bin/spn -> $(BIN)"
else
	@echo "cross binary: $(BIN)"
endif

build: configure
	@cmake --build $(WORK) --parallel $(shell nproc)

configure: fetch
	@cmake -S $(ROOT) -B $(WORK) -DTRIPLE=$(TRIPLE)

fetch:
	@cmake -P $(ROOT)/tools/cmake/fetch.cmake

test: build
	@ctest --test-dir $(WORK) --output-on-failure

install: build
	@mkdir -p $(HOME)/.local/bin
	cp $(BIN) $(HOME)/.local/bin/

uninstall:
	rm -f $(HOME)/.local/bin/spn

clean:
	rm -rf $(BUILD) $(ROOT)/bootstrap $(ROOT)/compile_commands.json
