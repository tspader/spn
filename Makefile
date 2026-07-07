ROOT        := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
HOST_TRIPLE := $(shell uname -m)-linux-gnu
TRIPLE      ?= $(HOST_TRIPLE)

BUILD     := $(ROOT)/.build
WORK      := $(BUILD)/work/$(TRIPLE)
WORK_HOST := $(BUILD)/work/$(HOST_TRIPLE)
STORE     := $(BUILD)/store/$(TRIPLE)

EXE :=
ifneq (,$(findstring windows,$(TRIPLE)))
EXE := .exe
endif
BIN := $(STORE)/bin/spn$(EXE)

.PHONY: all build configure fetch test smoke install uninstall clean nuke
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

ifeq ($(TRIPLE),$(HOST_TRIPLE))
configure: fetch
	@cmake -S $(ROOT) -B $(WORK) -DTRIPLE=$(TRIPLE) -DHOST_TRIPLE=$(HOST_TRIPLE)
else
.PHONY: host-tools
host-tools: fetch
	@cmake -S $(ROOT) -B $(WORK_HOST) -DTRIPLE=$(HOST_TRIPLE) -DHOST_TRIPLE=$(HOST_TRIPLE)
	@cmake --build $(WORK_HOST) --parallel $(shell nproc) --target embed jtd_gen
configure: host-tools
	@cmake -S $(ROOT) -B $(WORK) -DTRIPLE=$(TRIPLE) -DHOST_TRIPLE=$(HOST_TRIPLE) -DSPN_HOST_TOOLS=$(WORK_HOST)/tools
endif

fetch:
	@cmake -P $(ROOT)/tools/cmake/fetch.cmake

test: build
	@ctest --test-dir $(WORK) --output-on-failure

smoke: build
	@ctest --test-dir $(WORK) --output-on-failure -E 'graph|integration'

install: build
	@mkdir -p $(HOME)/.local/bin
	cp $(BIN) $(HOME)/.local/bin/

uninstall:
	rm -f $(HOME)/.local/bin/spn

clean:
	rm -rf $(BUILD)/work $(BUILD)/store $(ROOT)/bootstrap $(ROOT)/compile_commands.json

nuke: clean
	rm -rf $(BUILD) $(ROOT)/.cache/zig
