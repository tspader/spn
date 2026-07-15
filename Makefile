ROOT := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

ifeq ($(OS),Windows_NT)
  ifeq ($(PROCESSOR_ARCHITECTURE),ARM64)
    HOST_ARCH := aarch64
  else
    HOST_ARCH := x86_64
  endif
  HOST_TRIPLE := $(HOST_ARCH)-windows-msvc
  NPROC := $(NUMBER_OF_PROCESSORS)
  HOME ?= $(USERPROFILE)
  GENERATOR ?= Visual Studio 17 2022
  GEN_FLAGS := -G "$(GENERATOR)"
else
  UNAME_M := $(shell uname -m)
  ifeq ($(UNAME_M),arm64)
    UNAME_M := aarch64
  endif
  ifeq ($(shell uname -s),Darwin)
    HOST_TRIPLE := $(UNAME_M)-macos-none
  else
    HOST_TRIPLE := $(UNAME_M)-linux-gnu
  endif
  NPROC := $(shell getconf _NPROCESSORS_ONLN)
  GEN_FLAGS :=
endif

TRIPLE ?= $(HOST_TRIPLE)
CONFIG ?= Debug
SANITIZE ?=

ifeq ($(OS),Windows_NT)
  ifneq ($(TRIPLE),$(HOST_TRIPLE))
    $(error cross compiling from Windows is not supported; build natively with TRIPLE=$(HOST_TRIPLE))
  endif
endif

FLAVOR :=
ifneq ($(SANITIZE),)
  ifneq ($(TRIPLE),$(HOST_TRIPLE))
    $(error sanitized builds are host-only; drop TRIPLE or SANITIZE)
  endif
  FLAVOR := -san
endif

BUILD := $(ROOT)/.build
WORK := $(BUILD)/work/$(TRIPLE)$(FLAVOR)
WORK_HOST := $(BUILD)/work/$(HOST_TRIPLE)
STORE := $(BUILD)/store/$(TRIPLE)$(FLAVOR)

EXE :=
ifneq (,$(findstring windows,$(TRIPLE)))
  EXE := .exe
endif
BIN := $(STORE)/bin/spn$(EXE)

.PHONY: all build configure fetch test fuzz smoke install uninstall clean nuke
all: build
ifeq ($(OS),Windows_NT)
	@echo host binary: $(BIN)
else ifneq ($(SANITIZE),)
	@echo "sanitized binary: $(BIN)"
else ifeq ($(TRIPLE),$(HOST_TRIPLE))
	@ln -sfn .build/store/$(TRIPLE) $(ROOT)/bootstrap
	@ln -sf .build/work/$(TRIPLE)/compile_commands.json $(ROOT)/compile_commands.json
	@echo "host binary: bootstrap/bin/spn -> $(BIN)"
else
	@echo "cross binary: $(BIN)"
endif

build: configure
	@cmake --build $(WORK) --parallel $(NPROC) --config $(CONFIG)

ifeq ($(TRIPLE),$(HOST_TRIPLE))
configure: fetch
	@cmake -S $(ROOT) -B $(WORK) $(GEN_FLAGS) -DTRIPLE=$(TRIPLE) -DHOST_TRIPLE=$(HOST_TRIPLE) -DSPN_SANITIZE=$(if $(SANITIZE),ON,OFF)
else
.PHONY: host-tools
host-tools: fetch
	@cmake -S $(ROOT) -B $(WORK_HOST) -DTRIPLE=$(HOST_TRIPLE) -DHOST_TRIPLE=$(HOST_TRIPLE)
	@cmake --build $(WORK_HOST) --parallel $(NPROC) --target embed jtd_gen
configure: host-tools
	@cmake -S $(ROOT) -B $(WORK) -DTRIPLE=$(TRIPLE) -DHOST_TRIPLE=$(HOST_TRIPLE) -DSPN_HOST_TOOLS=$(WORK_HOST)/tools
endif

fetch:
	@cmake -P $(ROOT)/tools/cmake/fetch.cmake

test: build
	@ctest --test-dir $(WORK) -C $(CONFIG) --output-on-failure -E "^fuzz"

fuzz: build
	@ctest --test-dir $(WORK) -C $(CONFIG) --output-on-failure -R "^fuzz"

smoke: build
	@ctest --test-dir $(WORK) -C $(CONFIG) --output-on-failure -E "graph|integration|^fuzz"

install: build
	@cmake -E make_directory $(HOME)/.local/bin
	cmake -E copy $(BIN) $(HOME)/.local/bin/

uninstall:
	cmake -E rm -f $(HOME)/.local/bin/spn$(EXE)

clean:
	cmake -E rm -rf $(BUILD)/work $(BUILD)/store $(ROOT)/bootstrap $(ROOT)/compile_commands.json

nuke: clean
	cmake -E rm -rf $(BUILD) $(ROOT)/.cache/zig
