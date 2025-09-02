SPN_DIR_BUILD:= build
  SPN_DIR_BUILD_BOOTSTRAP:= $(SPN_DIR_BUILD)/bootstrap
		SPN_BOOTSTRAP_BIN := $(SPN_DIR_BUILD_BOOTSTRAP)/bin
			SPN_BOOTSTRAP_BIN_SDL := $(SPN_BOOTSTRAP_BIN)/libSDL3.so
		SPN_BOOTSTRAP_SDL := $(SPN_DIR_BUILD_BOOTSTRAP)/SDL
		SPN_BOOTSTRAP_SP := $(SPN_DIR_BUILD_BOOTSTRAP)/sp
		SPN_BOOTSTRAP_ARGPARSE := $(SPN_DIR_BUILD_BOOTSTRAP)/argparse
		SPN_BOOTSTRAP_TOML := $(SPN_DIR_BUILD_BOOTSTRAP)/toml
  SPN_DIR_BUILD_EXAMPLES := $(SPN_DIR_BUILD)/examples
  SPN_DIR_BUILD_OUTPUT := $(SPN_DIR_BUILD)/bin
    SPN_BINARY := $(SPN_DIR_BUILD_OUTPUT)/spn
    SPN_TEST_BINARY := $(SPN_DIR_BUILD_OUTPUT)/spn-test
SPN_DIR_SOURCE := source
SPN_DIR_TEST := test
SPN_MAKEFILE := Makefile
SPN_COMPILE_DB := compile_commands.json
SPN_CLANGD := .clangd
SPN_DIR_CACHE := ~/.cache/spn
SPN_INSTALL_PREFIX ?= $(HOME)/.local/bin

BUILD_TYPE ?= debug
CMAKE_TYPE := Debug
MAKEFLAGS += -j8
CC := bear --append -- gcc
MAKE := bear --append -- make
CMAKE := bear --append -- cmake


FLAG_LANGUAGE := -std=c11
FLAG_INCLUDES :=  -I$(SPN_DIR_SOURCE)
FLAG_OUTPUT := -o $(SPN_BINARY)
FLAG_OPTIMIZATION := -g
CC_FLAGS := $(FLAG_LANGUAGE) $(FLAG_INCLUDES) $(FLAG_OUTPUT) $(FLAG_OPTIMIZATION)

TEST_FLAG_OUTPUT := -o $(SPN_TEST_BINARY)
SPN_TEST_CC_FLAGS := $(FLAG_LANGUAGE) $(FLAG_INCLUDES) $(TEST_FLAG_OUTPUT) $(FLAG_OPTIMIZATION)


SPN_CLANGD_HEADER_ONLY_BULLSHIT := -DSP_OS_BACKEND_SDL, -DSP_IMPLEMENTATION, -DSPN_IMPLEMENTATION, -include, toml/toml.h, -include, sp/sp.h, -include, SDL3/SDL.h, -Wno-macro-redefined, -Wno-unused-includes

SDL_FLAG_DEFINES := -DCMAKE_BUILD_TYPE=$(CMAKE_TYPE) -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_EXAMPLES=OFF
SDL_CMAKE_FLAGS := $(SDL_FLAG_DEFINES)

IS_SPN_AVAILABLE := $(or $(shell which spn 2>/dev/null),$(wildcard ./build/bin/spn))
ifeq ($(IS_SPN_AVAILABLE),)
  DEFAULT_TARGET := bootstrap
  BOOTSTRAPPING_SPN := build/bin/spn
else
  DEFAULT_TARGET := build
  BOOTSTRAPPING_SPN := spn
endif
SPN := ./build/bin/spn

BOOTSTRAP_INCLUDE := -I$(SPN_BOOTSTRAP_SDL) -I$(SPN_BOOTSTRAP_SP) -I$(SPN_BOOTSTRAP_TOML) -I$(SPN_BOOTSTRAP_ARGPARSE)
BOOTSTRAP_LIB_INCLUDE := -L$(SPN_BOOTSTRAP_BIN)
BOOTSTRAP_LIBS := -lSDL3
BOOTSTRAP_FLAGS := $(BOOTSTRAP_INCLUDE) $(BOOTSTRAP_LIBS) $(BOOTSTRAP_LIB_INCLUDE)

.PHONY: all build sdl clangd clean examples test install uninstall
.NOTPARALLEL: examples $(EXAMPLE_BINARIES)

all: $(DEFAULT_TARGET)

$(SPN_DIR_BUILD_OUTPUT):
	@mkdir -p $(SPN_DIR_BUILD_OUTPUT)

$(SPN_DIR_BUILD_EXAMPLES):
	@mkdir -p $(SPN_DIR_BUILD_EXAMPLES)

$(SPN_DIR_BUILD_BOOTSTRAP):
	@mkdir -p $(SPN_DIR_BUILD_BOOTSTRAP)

$(SPN_BINARY): $(SPN_DIR_SOURCE)/main.c $(SPN_DIR_SOURCE)/spn.h  | $(SPN_DIR_BUILD_OUTPUT)
	@echo ">> building spn"
	$(eval SPN_FLAGS := $(shell $(BOOTSTRAPPING_SPN) print --compiler gcc))
	$(CC) $(CC_FLAGS) $(SPN_FLAGS) $(SPN_DIR_SOURCE)/main.c

$(SPN_TEST_BINARY): $(SPN_DIR_TEST)/main.c $(SPN_DIR_SOURCE)/spn.h $(SPN_BINARY)
	$(eval SPN_FLAGS := $(shell $(SPN) print --compiler gcc))
	$(CC) $(SPN_TEST_CC_FLAGS) $(SPN_FLAGS) $(SPN_DIR_TEST)/main.c

$(SPN_COMPILE_DB): $(SPN_MAKEFILE)

$(SPN_CLANGD): $(SPN_COMPILE_DB)
	@printf "CompileFlags:\n  Add: [$(SPN_CLANGD_HEADER_ONLY_BULLSHIT)]\n" > $(SPN_CLANGD)

EXAMPLES := $(notdir $(wildcard examples/*))
EXAMPLE_DIRS := $(addprefix examples/, $(EXAMPLES))
EXAMPLE_BINARIES := $(addprefix build/examples/, $(EXAMPLES))
$(EXAMPLE_BINARIES): build/examples/%: examples/%/main.c examples/%/spn.toml | $(SPN_DIR_BUILD_EXAMPLES)
	$(eval BINARY := $@)
	$(eval EXAMPLE := $*)
	$(eval EXAMPLE_DIR := examples/$*)
	@echo
	@echo ">> building $(EXAMPLE_DIR)"

	$(SPN) --lock -C $(EXAMPLE_DIR) build
	$(eval SPN_FLAGS := $(shell $(SPN) -C $(EXAMPLE_DIR) print --compiler gcc))
	$(CC) $(EXAMPLE_DIR)/main.c -o $(BINARY) $(SPN_FLAGS) -lm


build: $(SPN_BINARY)

test: build $(SPN_TEST_BINARY)
	@$(SPN_TEST_BINARY)

examples: $(DEFAULT_TARGET) $(EXAMPLE_BINARIES)

install: $(DEFAULT_TARGET)
	@mkdir -p $(SPN_INSTALL_PREFIX)
	@cp $(SPN_BINARY) $(SPN_INSTALL_PREFIX)/spn
	@echo "Installed spn to $(SPN_INSTALL_PREFIX)/spn"

uninstall:
	@rm -f $(SPN_INSTALL_PREFIX)/spn

clangd: $(SPN_COMPILE_DB) $(SPN_CLANGD)

clean:
	@rm -rf $(SPN_DIR_BUILD)
	@rm -f $(SPN_COMPILE_DB)
	@rm -f $(SPN_CLANGD)


$(SPN_BOOTSTRAP_SDL): | $(SPN_DIR_BUILD_BOOTSTRAP)
	@git clone git@github.com:libsdl-org/SDL.git $(SPN_BOOTSTRAP_SDL)

$(SPN_BOOTSTRAP_SP): | $(SPN_DIR_BUILD_BOOTSTRAP)
	@git clone git@github.com:tspader/sp.git $(SPN_BOOTSTRAP_SP)

$(SPN_BOOTSTRAP_TOML): | $(SPN_DIR_BUILD_BOOTSTRAP)
	@git clone git@github.com:tspader/toml.git $(SPN_BOOTSTRAP_TOML)

$(SPN_BOOTSTRAP_ARGPARSE): | $(SPN_DIR_BUILD_BOOTSTRAP)
	@git clone git@github.com:tspader/argparse.git $(SPN_BOOTSTRAP_ARGPARSE)

$(SPN_BOOTSTRAP_BIN)/libSDL3.so: | $(SPN_BOOTSTRAP_SDL)
	$(eval SDL_FLAGS := -DCMAKE_BUILD_TYPE=Debug -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_EXAMPLES=OFF)
	cmake -S$(SPN_BOOTSTRAP_SDL) -B$(SPN_BOOTSTRAP_BIN) $(SDL_FLAGS)
	cmake --build $(SPN_BOOTSTRAP_BIN) --parallel

bootstrap: $(SPN_BOOTSTRAP_BIN_SDL) | $(SPN_BOOTSTRAP_SP) $(SPN_BOOTSTRAP_ARGPARSE) $(SPN_BOOTSTRAP_TOML) $(SPN_DIR_BUILD_OUTPUT)
	@echo ">> bootstrapping spn"
	$(CC) $(CC_FLAGS) $(BOOTSTRAP_FLAGS) $(SPN_DIR_SOURCE)/main.c
	@echo
	@echo ">> building with bootstrapped binary"
	$(MAKE)
	@echo
	@echo ">> done! try 'make examples' to build some projects with your spn binary"

