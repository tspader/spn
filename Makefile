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
  ifndef GENERATOR
    GENERATOR := $(shell powershell -NoProfile -ExecutionPolicy Bypass -File "$(ROOT)/tools/cmake/find_vs_generator.ps1" 2>NUL)
    ifeq ($(strip $(GENERATOR)),)
      GENERATOR := Visual Studio 17 2022
    endif
  endif
  GEN_FLAGS :=
  ifneq ($(GENERATOR),)
    GEN_FLAGS := -G "$(GENERATOR)"
  endif
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

ifeq ($(CONFIG),Debug)
  CONFIG_TAG :=
else ifeq ($(CONFIG),Release)
  CONFIG_TAG := -release
else
  $(error unsupported CONFIG '$(CONFIG)'; use Debug or Release)
endif

BUILD := $(ROOT)/.build
WORK := $(BUILD)/work/$(TRIPLE)$(FLAVOR)$(CONFIG_TAG)
WORK_HOST := $(BUILD)/work/$(HOST_TRIPLE)
STORE := $(BUILD)/store/$(TRIPLE)$(FLAVOR)$(CONFIG_TAG)

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
	@ln -sfn .build/store/$(TRIPLE)$(CONFIG_TAG) $(ROOT)/bootstrap
	@ln -sf .build/work/$(TRIPLE)$(CONFIG_TAG)/compile_commands.json $(ROOT)/compile_commands.json
	@echo "host binary: bootstrap/bin/spn -> $(BIN)"
else
	@echo "cross binary: $(BIN)"
endif

build: configure
	@cmake --build $(WORK) --parallel $(NPROC) --config $(CONFIG)

ifeq ($(TRIPLE),$(HOST_TRIPLE))
configure: fetch
	@cmake -S $(ROOT) -B $(WORK) $(GEN_FLAGS) -DTRIPLE=$(TRIPLE) -DHOST_TRIPLE=$(HOST_TRIPLE) -DCMAKE_BUILD_TYPE=$(CONFIG) -DSPN_SANITIZE=$(if $(SANITIZE),ON,OFF)
else
.PHONY: host-tools
host-tools: fetch
	@cmake -S $(ROOT) -B $(WORK_HOST) -DTRIPLE=$(HOST_TRIPLE) -DHOST_TRIPLE=$(HOST_TRIPLE)
	@cmake --build $(WORK_HOST) --parallel $(NPROC) --target embed jtd_gen installer_render
configure: host-tools
	@cmake -S $(ROOT) -B $(WORK) -DTRIPLE=$(TRIPLE) -DHOST_TRIPLE=$(HOST_TRIPLE) -DCMAKE_BUILD_TYPE=$(CONFIG) -DSPN_HOST_TOOLS=$(WORK_HOST)/tools
endif

fetch:
	@cmake -P $(ROOT)/tools/cmake/fetch.cmake

test: build
	@ctest --test-dir $(WORK) -C $(CONFIG) --output-on-failure -E "^fuzz"

fuzz: build
	@ctest --test-dir $(WORK) -C $(CONFIG) --output-on-failure -R "^fuzz"

CI_TRIPLES ?= $(HOST_TRIPLE)
ifeq ($(HOST_TRIPLE),x86_64-linux-gnu)
  CI_TRIPLES := x86_64-linux-gnu x86_64-linux-musl
endif

.PHONY: ci
ci: export SPN_CONFIG_DIR := $(BUILD)/ci-config
ci: $(addprefix ci-,$(CI_TRIPLES))
	$(BIN) build
	$(BIN) test

ci-%:
	@$(MAKE) all test TRIPLE=$*

SELFHOST_PROFILES := default
ifeq ($(findstring linux,$(HOST_TRIPLE)),linux)
  SELFHOST_PROFILES := gnu musl
else ifeq ($(OS),Windows_NT)
  SELFHOST_PROFILES := msvc mingw
endif

STAGE0 = $(shell sh $(ROOT)/tools/stage0.sh)

.PHONY: stage0 ci-selfhost
stage0:
	@sh $(ROOT)/tools/stage0.sh

ci-selfhost: export SPN_CONFIG_DIR := $(BUILD)/ci-config
ci-selfhost: stage0 $(addprefix selfhost-,$(SELFHOST_PROFILES))
ifeq ($(OS),Windows_NT)
ci-selfhost: selfhost-msvc-matrix
endif

selfhost-%: stage0
	@"$(STAGE0)" build -p $*
	@"$(STAGE0)" test -p $*

.PHONY: selfhost-msvc-matrix
selfhost-msvc-matrix: export SPN_TEST_TOOLCHAIN := msvc
selfhost-msvc-matrix: stage0
	@"$(STAGE0)" test -p msvc integration

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
