ifeq ($(OS),Windows_NT)
  CC := gcc
  MAKE := make
  CMAKE := cmake
  SDL := SDL3.lib
  LUAJIT := lua51.lib
  SPN_EXE := spn.exe
else
  CC := bear --append -- gcc
  MAKE := bear --append -- make
  CMAKE := bear --append -- cmake
  SDL := libSDL3.a
  LUAJIT := libluajit.a
  SPN_EXE := spn
endif

PREINSTALLED_SPN := $(SPN_EXE)
PREBUILT_SPN := ./build/bin/$(SPN_EXE)

IS_SPN_PREINSTALLED := $(shell which $(PREINSTALLED_SPN))
IS_SPN_PREBUILT := $(wildcard $(PREBUILT_SPN))
ifneq ($(IS_SPN_PREINSTALLED),)
  DEFAULT_TARGET := build
  BOOTSTRAPPED_SPN := $(PREINSTALLED_SPN)
else ifneq ($(IS_SPN_PREBUILT),)
  DEFAULT_TARGET := build
  BOOTSTRAPPED_SPN := $(PREBUILT_SPN)
else
  DEFAULT_TARGET := bootstrap
endif

SPN_DIR_BUILD:= build
  SPN_DIR_BUILD_BOOTSTRAP:= $(SPN_DIR_BUILD)/bootstrap
		SPN_BOOTSTRAP_WORK := $(SPN_DIR_BUILD_BOOTSTRAP)/work
		SPN_BOOTSTRAP_BIN := $(SPN_DIR_BUILD_BOOTSTRAP)/bin
			SPN_BOOTSTRAP_SDL_BINARY := $(SPN_BOOTSTRAP_BIN)/$(SDL)
			SPN_BOOTSTRAP_LUAJIT_BINARY := $(SPN_BOOTSTRAP_BIN)/$(LUAJIT)
		SPN_BOOTSTRAP_SDL := $(SPN_DIR_BUILD_BOOTSTRAP)/SDL
		SPN_BOOTSTRAP_SP := $(SPN_DIR_BUILD_BOOTSTRAP)/sp
		SPN_BOOTSTRAP_ARGPARSE := $(SPN_DIR_BUILD_BOOTSTRAP)/argparse
		SPN_BOOTSTRAP_LUAJIT := $(SPN_DIR_BUILD_BOOTSTRAP)/luajit
		SPN_BOOTSTRAP_TOML := $(SPN_DIR_BUILD_BOOTSTRAP)/toml
		SPN_BOOTSTRAP_W64DEVKIT := $(SPN_DIR_BUILD_BOOTSTRAP)/w64devkit
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



FLAG_LANGUAGE := -std=c11
FLAG_INCLUDES :=  -I$(SPN_DIR_SOURCE)
FLAG_OUTPUT := -o $(SPN_BINARY)
FLAG_OPTIMIZATION := -g -rdynamic
CC_FLAGS := $(FLAG_LANGUAGE) $(FLAG_INCLUDES) $(FLAG_OUTPUT) $(FLAG_OPTIMIZATION)

TEST_FLAG_OUTPUT := -o $(SPN_TEST_BINARY)
SPN_TEST_CC_FLAGS := $(FLAG_LANGUAGE) $(FLAG_INCLUDES) $(TEST_FLAG_OUTPUT) $(FLAG_OPTIMIZATION)


SPN_CLANGD_HEADER_ONLY_BULLSHIT := -DSP_OS_BACKEND_SDL, -DSP_IMPLEMENTATION, -DSPN_IMPLEMENTATION, -include, toml/toml.h, -include, sp/sp.h, -include, SDL3/SDL.h, -Wno-macro-redefined, -Wno-unused-includes

SDL_FLAG_DEFINES := -DCMAKE_BUILD_TYPE=$(CMAKE_TYPE) -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_EXAMPLES=OFF
SDL_CMAKE_FLAGS := $(SDL_FLAG_DEFINES)


.PHONY: all build sdl clangd clean examples test install uninstall
.NOTPARALLEL: examples $(EXAMPLE_BINARIES)

all: $(DEFAULT_TARGET)

$(SPN_DIR_BUILD_OUTPUT):
	@mkdir -p $(SPN_DIR_BUILD_OUTPUT)

$(SPN_DIR_BUILD_EXAMPLES):
	@mkdir -p $(SPN_DIR_BUILD_EXAMPLES)

$(SPN_BINARY): $(SPN_DIR_SOURCE)/main.c $(SPN_DIR_SOURCE)/spn.h  | $(SPN_DIR_BUILD_OUTPUT)
	@echo
	@echo ">> building spn"
	$(CC) $(CC_FLAGS) $(shell $(BOOTSTRAPPED_SPN) print --compiler gcc) ./source/main.c

$(SPN_TEST_BINARY): $(SPN_DIR_TEST)/main.c $(SPN_DIR_SOURCE)/spn.h $(SPN_BINARY)
	@echo
	@echo ">> building unit tests"
	$(CC) $(SPN_TEST_CC_FLAGS) $(shell $(PREBUILT_SPN) print --compiler gcc) ./test/main.c

$(SPN_COMPILE_DB): $(SPN_MAKEFILE)

$(SPN_CLANGD): $(SPN_COMPILE_DB)
	@printf "CompileFlags:\n  Add: [$(SPN_CLANGD_HEADER_ONLY_BULLSHIT)]\n" > $(SPN_CLANGD)

EXAMPLES := $(notdir $(wildcard examples/*))
EXAMPLE_DIRS := $(addprefix examples/, $(EXAMPLES))
EXAMPLE_BINARIES := $(addprefix build/examples/, $(EXAMPLES))
$(EXAMPLE_BINARIES): build/examples/%: examples/%/main.c examples/%/spn.lua | $(SPN_DIR_BUILD_EXAMPLES)
	$(eval BINARY := $@)
	$(eval EXAMPLE := $*)
	$(eval EXAMPLE_DIR := examples/$*)
	@echo
	@echo ">> building $(EXAMPLE_DIR)"

	$(BOOTSTRAPPED_SPN) --lock -C $(EXAMPLE_DIR) build
	$(CC) $(EXAMPLE_DIR)/main.c -o $(BINARY) $(shell $(BOOTSTRAPPED_SPN) -C $(EXAMPLE_DIR) print --compiler gcc) -lm

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



BOOTSTRAP_INCLUDE := -I$(SPN_BOOTSTRAP_SDL)/include -I$(SPN_BOOTSTRAP_SP) -I$(SPN_BOOTSTRAP_TOML) -I$(SPN_BOOTSTRAP_ARGPARSE) -I$(SPN_BOOTSTRAP_LUAJIT)/src
ifeq ($(OS),Windows_NT)
  BOOTSTRAP_LIBS := $(SPN_BOOTSTRAP_SDL_BINARY) $(SPN_BOOTSTRAP_LUAJIT)/src/lua51.dll
else
  BOOTSTRAP_LIBS := $(SPN_BOOTSTRAP_SDL_BINARY) $(SPN_BOOTSTRAP_LUAJIT)/src/libluajit.a -lm -ldl
endif
BOOTSTRAP_FLAGS := $(BOOTSTRAP_INCLUDE) $(BOOTSTRAP_LIBS)

$(SPN_DIR_BUILD_BOOTSTRAP):
	@mkdir -p $(SPN_DIR_BUILD_BOOTSTRAP)

$(SPN_BOOTSTRAP_BIN): $(SPN_DIR_BUILD_BOOTSTRAP)
	@mkdir -p $(SPN_BOOTSTRAP_BIN)

$(SPN_BOOTSTRAP_WORK): $(SPN_DIR_BUILD_BOOTSTRAP)
	@mkdir -p $(SPN_BOOTSTRAP_WORK)

$(SPN_BOOTSTRAP_SDL): | $(SPN_DIR_BUILD_BOOTSTRAP)
	@git clone git@github.com:libsdl-org/SDL.git $(SPN_BOOTSTRAP_SDL)

$(SPN_BOOTSTRAP_SP): | $(SPN_DIR_BUILD_BOOTSTRAP)
	@git clone git@github.com:tspader/sp.git $(SPN_BOOTSTRAP_SP)

$(SPN_BOOTSTRAP_TOML): | $(SPN_DIR_BUILD_BOOTSTRAP)
	@git clone git@github.com:tspader/toml.git $(SPN_BOOTSTRAP_TOML)

$(SPN_BOOTSTRAP_ARGPARSE): | $(SPN_DIR_BUILD_BOOTSTRAP)
	@git clone git@github.com:tspader/argparse.git $(SPN_BOOTSTRAP_ARGPARSE)

$(SPN_BOOTSTRAP_LUAJIT): | $(SPN_DIR_BUILD_BOOTSTRAP)
	@git clone git@github.com:LuaJIT/LuaJIT.git $(SPN_BOOTSTRAP_LUAJIT)

$(SPN_BOOTSTRAP_LUAJIT_BINARY): | $(SPN_BOOTSTRAP_LUAJIT) $(SPN_BOOTSTRAP_BIN)
	@make -C $(SPN_BOOTSTRAP_LUAJIT) amalg
	@cp $(SPN_BOOTSTRAP_LUAJIT)/src/$(LUAJIT) $(SPN_BOOTSTRAP_LUAJIT_BINARY)

$(SPN_BOOTSTRAP_SDL_BINARY): | $(SPN_BOOTSTRAP_SDL) $(SPN_BOOTSTRAP_WORK) $(SPN_BOOTSTRAP_BIN)
	$(eval SDL_FLAGS := -DCMAKE_BUILD_TYPE=Debug -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL_EXAMPLES=OFF)
	cmake -S$(SPN_BOOTSTRAP_SDL) -B$(SPN_BOOTSTRAP_WORK) $(SDL_FLAGS)
	cmake --build $(SPN_BOOTSTRAP_WORK) --parallel
	@cp $(SPN_BOOTSTRAP_WORK)/$(SDL) $(SPN_BOOTSTRAP_SDL_BINARY) 2>/dev/null

SPN_BOOTSTRAP_DEPS := $(SPN_BOOTSTRAP_SP) $(SPN_BOOTSTRAP_ARGPARSE) $(SPN_BOOTSTRAP_TOML) $(SPN_DIR_BUILD_OUTPUT)

bootstrap: $(SPN_BOOTSTRAP_SDL_BINARY) $(SPN_BOOTSTRAP_LUAJIT_BINARY) | $(SPN_BOOTSTRAP_DEPS)
	@echo ">> bootstrapping spn"
	$(CC) $(CC_FLAGS) $(BOOTSTRAP_INCLUDE) ./source/main.c $(BOOTSTRAP_LIBS)
	@echo
	@echo ">> building with bootstrapped binary"
	#$(MAKE)
	@echo
	@echo ">> done! try 'make examples' to build some projects with your spn binary"

