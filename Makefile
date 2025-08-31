SPN_DIR_BUILD:= build
  SPN_DIR_BUILD_EXAMPLES := $(SPN_DIR_BUILD)/examples
  SPN_DIR_BUILD_OUTPUT := $(SPN_DIR_BUILD)/bin
    SPN_BINARY := $(SPN_DIR_BUILD_OUTPUT)/spn
    SPN_TEST_BINARY := $(SPN_DIR_BUILD_OUTPUT)/spn-test
    SDL_BINARY := $(SPN_DIR_BUILD_OUTPUT)/libSDL3.so
  SPN_DIR_BUILD_SDL := $(SPN_DIR_BUILD)/sdl
SPN_DIR_SOURCE := source
SPN_DIR_EXTERNAL := external
  SPN_DIR_SDL := $(SPN_DIR_EXTERNAL)/SDL
    SPN_DIR_SDL_INCLUDE := $(SPN_DIR_SDL)/include
  SPN_DIR_SP := $(SPN_DIR_EXTERNAL)/sp
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
SPN := $(SPN_BINARY)

FLAG_LANGUAGE := -std=c11
FLAG_INCLUDES := -I$(SPN_DIR_EXTERNAL) -I$(SPN_DIR_SDL) -I$(SPN_DIR_SOURCE) $(shell spn flags include)
FLAG_OUTPUT := -o $(SPN_BINARY)
FLAG_OPTIMIZATION := -g
FLAG_LIBS := $(shell spn flags lib-include) $(shell spn flags libs)
CC_FLAGS := $(FLAG_LANGUAGE) $(FLAG_INCLUDES) $(FLAG_OUTPUT) $(FLAG_OPTIMIZATION) $(FLAG_LIBS)

TEST_FLAG_OUTPUT := -o $(SPN_TEST_BINARY)
SPN_TEST_CC_FLAGS := $(FLAG_LANGUAGE) $(FLAG_INCLUDES) $(TEST_FLAG_OUTPUT) $(FLAG_OPTIMIZATION) $(FLAG_LIBS)

SPN_DEPS := $(SPN_DIR_SOURCE)/spn.h $(SDL_BINARY)

SPN_CLANGD_HEADER_ONLY_BULLSHIT := -DSP_OS_BACKEND_SDL, -DSP_IMPLEMENTATION, -DSPN_IMPLEMENTATION, -include, toml/toml.h, -include, sp/sp.h, -include, SDL3/SDL.h, -Wno-macro-redefined, -Wno-unused-includes

SDL_FLAG_DEFINES := -DCMAKE_BUILD_TYPE=$(CMAKE_TYPE) -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_EXAMPLES=OFF
SDL_CMAKE_FLAGS := $(SDL_FLAG_DEFINES)


.PHONY: all

all: build clangd

$(SPN_DIR_BUILD_OUTPUT):
	@mkdir -p $(SPN_DIR_BUILD_OUTPUT)

$(SPN_DIR_BUILD_SDL):
	@mkdir -p $(SPN_DIR_BUILD_SDL)

$(SDL_BINARY): $(SPN_DIR_BUILD_SDL) | $(SPN_DIR_BUILD_OUTPUT)
	$(CMAKE) -S$(SPN_DIR_SDL) -B$(SPN_DIR_BUILD_SDL) $(SDL_CMAKE_FLAGS)
	$(CMAKE) --build $(SPN_DIR_BUILD_SDL) --parallel
	cp $(SPN_DIR_BUILD_SDL)/libSDL3.so $(SPN_DIR_BUILD_OUTPUT)/libSDL3.so

$(SPN_BINARY): $(SPN_DIR_SOURCE)/main.c $(SPN_DEPS)
	$(CC) $(CC_FLAGS) $(SPN_DIR_SOURCE)/main.c

$(SPN_TEST_BINARY): $(SPN_DIR_TEST)/main.c $(SPN_DEPS)
	$(CC) $(SPN_TEST_CC_FLAGS) $(SPN_DIR_TEST)/main.c

$(SPN_COMPILE_DB): $(SPN_MAKEFILE)

$(SPN_CLANGD): $(SPN_COMPILE_DB)
	@printf "CompileFlags:\n  Add: [$(SPN_CLANGD_HEADER_ONLY_BULLSHIT)]\n" > $(SPN_CLANGD)


.PHONY: build sdl clangd clean nuke test test-unit test-examples test-all install uninstall all

build: $(SPN_BINARY)

test-unit: $(SPN_TEST_BINARY)
	$(SPN_TEST_BINARY)

test-examples: build
	@$(SPN_BINARY) --no-interactive --use-lockfile -C ./examples/hello build
	@$(SPN_BINARY) --no-interactive --use-lockfile -C ./examples/sdl build
	@$(SPN_BINARY) --no-interactive --use-lockfile -C ./examples/sqlite build
	@echo "All example tests passed!"

test: test-unit test-examples

test-all: clean test

sdl: $(SDL_BINARY)

install: build
	@mkdir -p $(SPN_INSTALL_PREFIX)
	@cp $(SPN_BINARY) $(SPN_INSTALL_PREFIX)/spn
	@echo "Installed spn to $(SPN_INSTALL_PREFIX)/spn"

uninstall:
	@rm -f $(SPN_INSTALL_PREFIX)/spn

clangd: $(SPN_COMPILE_DB) $(SPN_CLANGD)

clean:
	@rm -rf $(SPN_DIR_BUILD_OUTPUT)
	@rm -rf $(SPN_DIR_BUILD_EXAMPLES)
	@rm -f $(SPN_COMPILE_DB)
	@rm -f $(SPN_CLANGD)

nuke:
	@rm -rf $(SPN_DIR_BUILD)
	@rm -f $(SPN_COMPILE_DB)
	@rm -f $(SPN_CLANGD)
