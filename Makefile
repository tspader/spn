#########
# UTILS #
#########
ANSI_FG_CYAN := \033[36m
ANSI_FG_BRIGHT_BLACK := \033[90m
ANSI_FG_BRIGHT_CYAN := \033[96m
ANSI_FG_BRIGHT_YELLOW := \033[93m
ANSI_RESET := \033[0m

define print_heading
	@printf "$(ANSI_FG_BRIGHT_CYAN)>> $(ANSI_RESET)"
endef

define print_and_run
	@printf "$(ANSI_FG_BRIGHT_CYAN)>>> $(ANSI_RESET)"
	@printf "$(ANSI_FG_BRIGHT_YELLOW)$1$(ANSI_RESET)"
	@echo
	@$1
endef

############
# PLATFORM #
############
ifeq ($(OS),Windows_NT)
  CC := gcc
  MAKE := make
  CMAKE := cmake
  SDL := SDL3.lib
  LUAJIT := lua51.lib
  SPN_EXE := spn.exe
  RPATH_FLAG :=
  IS_SPN_PREINSTALLED := $(shell where $(SPN_EXE) 2>NUL)
else
  CC := bear --append -- gcc
  MAKE := bear --append -- make
  CMAKE := bear --append -- cmake
  SDL := libSDL3.a
  LUAJIT := libluajit.a
  SPN_EXE := spn
  IS_SPN_PREINSTALLED := $(shell which $(SPN_EXE) 2>/dev/null)

ifeq ($(shell uname),Darwin)
  RPATH_FLAG := -Wl,-rpath,@loader_path
endif

ifeq ($(shell uname),Linux)
  RPATH_FLAG := -Wl,-rpath,\$$ORIGIN
endif
endif

#########
# PATHS #
#########
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
SPN_DIR_SOURCE := source
SPN_MAKEFILE := Makefile
SPN_COMPILE_DB := compile_commands.json
SPN_CLANGD := .clangd
SPN_DIR_CACHE := ~/.cache/spn
SPN_INSTALL_PREFIX ?= $(HOME)/.local/bin

#########
# FLAGS #
#########
BUILD_TYPE ?= debug
CMAKE_TYPE := Debug
MAKEFLAGS += -j8

FLAG_LANGUAGE := -std=c11
FLAG_INCLUDES :=  -I$(SPN_DIR_SOURCE)
FLAG_OUTPUT := -o $(SPN_BINARY)
FLAG_OPTIMIZATION := -g -rdynamic
FLAG_LIBS := -lm -lpthread -lelf -ldl
CC_FLAGS := $(FLAG_LANGUAGE) $(FLAG_OPTIMIZATION) $(FLAG_INCLUDES) $(FLAG_LIBS) $(FLAG_OUTPUT)

SPN_CLANGD_HEADER_ONLY_BULLSHIT := -DSP_OS_BACKEND_SDL, -DSP_IMPLEMENTATION, -DSPN_IMPLEMENTATION, -include, toml/toml.h, -include, sp/sp.h, -include, SDL3/SDL.h, -Wno-macro-redefined, -Wno-unused-includes

###############
# ENTRY POINT #
###############
PREINSTALLED_SPN := $(SPN_EXE)
PREBUILT_SPN := ./build/bin/$(SPN_EXE)

IS_SPN_PREBUILT := $(wildcard $(PREBUILT_SPN))
ifneq ($(IS_SPN_PREBUILT),)
  DEFAULT_TARGET := build
  BOOTSTRAPPED_SPN := $(PREBUILT_SPN)
else ifneq ($(IS_SPN_PREINSTALLED),)
  DEFAULT_TARGET := build
  BOOTSTRAPPED_SPN := $(PREINSTALLED_SPN)
else
  DEFAULT_TARGET := bootstrap
endif

.PHONY: all build sdl clangd clean examples install uninstall
.NOTPARALLEL: examples $(EXAMPLE_BINARIES)

all: $(DEFAULT_TARGET)

#######
# SPN #
#######
$(SPN_DIR_BUILD_OUTPUT):
	@mkdir -p $(SPN_DIR_BUILD_OUTPUT)

$(SPN_DIR_BUILD_EXAMPLES):
	@mkdir -p $(SPN_DIR_BUILD_EXAMPLES)

$(SPN_BINARY): $(SPN_DIR_SOURCE)/main.c $(SPN_DIR_SOURCE)/spn.h  | $(SPN_DIR_BUILD_OUTPUT)
	$(call print_heading)
	@printf "building $(ANSI_FG_BRIGHT_CYAN)spn$(ANSI_RESET)"
	@echo

	$(call print_and_run,$(BOOTSTRAPPED_SPN) build)
	$(call print_and_run,$(CC) $(CC_FLAGS) $$($(BOOTSTRAPPED_SPN) print --compiler gcc) ./source/main.c)

$(SPN_COMPILE_DB): $(SPN_MAKEFILE)

$(SPN_CLANGD): $(SPN_COMPILE_DB)
	@printf "CompileFlags:\n  Add: [$(SPN_CLANGD_HEADER_ONLY_BULLSHIT)]\n" > $(SPN_CLANGD)

#############
# BOOTSTRAP #
#############
ifeq ($(OS),Windows_NT)
  BOOTSTRAP_LIBS := $(SPN_BOOTSTRAP_SDL_BINARY) $(SPN_BOOTSTRAP_LUAJIT)/src/lua51.dll
else ifeq ($(shell uname),Darwin)
  MACOSX_DEPLOYMENT_TARGET := 15.0
  MACOS_FRAMEWORKS := -framework CoreFoundation -framework Foundation -framework Cocoa \
                      -framework IOKit -framework GameController -framework ForceFeedback \
                      -framework AVFoundation -framework CoreAudio -framework AudioToolbox \
                      -framework Metal -framework MetalKit -framework QuartzCore \
                      -framework CoreHaptics -framework CoreMedia -framework Carbon -framework UniformTypeIdentifiers
  BOOTSTRAP_LIBS := $(SPN_BOOTSTRAP_SDL_BINARY) $(SPN_BOOTSTRAP_LUAJIT)/src/libluajit.a -lm -ldl $(MACOS_FRAMEWORKS)

	export MACOSX_DEPLOYMENT_TARGET
else
  BOOTSTRAP_LIBS := $(SPN_BOOTSTRAP_SDL_BINARY) $(SPN_BOOTSTRAP_LUAJIT)/src/libluajit.a
endif

BOOTSTRAP_INCLUDE := -I$(SPN_BOOTSTRAP_SDL)/include -I$(SPN_BOOTSTRAP_SP) -I$(SPN_BOOTSTRAP_TOML) -I$(SPN_BOOTSTRAP_ARGPARSE) -I$(SPN_BOOTSTRAP_LUAJIT)/src
BOOTSTRAP_FLAGS := $(BOOTSTRAP_INCLUDE) $(FLAG_LIBS) $(BOOTSTRAP_LIBS)

$(SPN_DIR_BUILD_BOOTSTRAP):
	@mkdir -p $(SPN_DIR_BUILD_BOOTSTRAP)

$(SPN_BOOTSTRAP_BIN): $(SPN_DIR_BUILD_BOOTSTRAP)
	@mkdir -p $(SPN_BOOTSTRAP_BIN)

$(SPN_BOOTSTRAP_WORK): $(SPN_DIR_BUILD_BOOTSTRAP)
	@mkdir -p $(SPN_BOOTSTRAP_WORK)

$(SPN_BOOTSTRAP_SDL): | $(SPN_DIR_BUILD_BOOTSTRAP)
	@git clone https://github.com/libsdl-org/SDL.git $(SPN_BOOTSTRAP_SDL)

$(SPN_BOOTSTRAP_SP): | $(SPN_DIR_BUILD_BOOTSTRAP)
	@git clone https://github.com/tspader/sp.git $(SPN_BOOTSTRAP_SP)

$(SPN_BOOTSTRAP_ARGPARSE): | $(SPN_DIR_BUILD_BOOTSTRAP)
	@git clone https://github.com/tspader/argparse.git $(SPN_BOOTSTRAP_ARGPARSE)

$(SPN_BOOTSTRAP_LUAJIT): | $(SPN_DIR_BUILD_BOOTSTRAP)
	@git clone https://github.com/LuaJIT/LuaJIT.git $(SPN_BOOTSTRAP_LUAJIT)

$(SPN_BOOTSTRAP_LUAJIT_BINARY): | $(SPN_BOOTSTRAP_LUAJIT) $(SPN_BOOTSTRAP_BIN)
	@make -C $(SPN_BOOTSTRAP_LUAJIT) amalg
	@cp $(SPN_BOOTSTRAP_LUAJIT)/src/$(LUAJIT) $(SPN_BOOTSTRAP_LUAJIT_BINARY)

$(SPN_BOOTSTRAP_SDL_BINARY): | $(SPN_BOOTSTRAP_SDL) $(SPN_BOOTSTRAP_WORK) $(SPN_BOOTSTRAP_BIN)
	$(eval SDL_FLAGS := -DCMAKE_BUILD_TYPE=Debug -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_UNIX_CONSOLE_BUILD=ON)
	cmake -S$(SPN_BOOTSTRAP_SDL) -B$(SPN_BOOTSTRAP_WORK) $(SDL_FLAGS)
	cmake --build $(SPN_BOOTSTRAP_WORK) --parallel
	@cp $(SPN_BOOTSTRAP_WORK)/$(SDL) $(SPN_BOOTSTRAP_SDL_BINARY) 2>/dev/null

SPN_BOOTSTRAP_DEPS := $(SPN_BOOTSTRAP_SP) $(SPN_BOOTSTRAP_ARGPARSE) $(SPN_DIR_BUILD_OUTPUT)

bootstrap: $(SPN_BOOTSTRAP_SDL_BINARY) $(SPN_BOOTSTRAP_LUAJIT_BINARY) | $(SPN_BOOTSTRAP_DEPS)
	@printf "$(ANSI_FG_BRIGHT_CYAN)>> $(ANSI_RESET)"
	@echo "bootstrapping spn"
	$(CC) $(CC_FLAGS) $(BOOTSTRAP_INCLUDE) ./source/main.c $(BOOTSTRAP_LIBS)
	@echo
	@printf "$(ANSI_FG_BRIGHT_CYAN)>> $(ANSI_RESET)"
	@printf "done! try $(ANSI_FG_BRIGHT_CYAN)make examples$(ANSI_RESET) to build some projects with your spn binary"
	@echo

############
# EXAMPLES #
############
EXAMPLES := $(notdir $(wildcard examples/*))
EXAMPLE_DIRS := $(addprefix examples/, $(EXAMPLES))
EXAMPLE_BINARIES := $(addprefix build/examples/, $(EXAMPLES))
$(EXAMPLE_BINARIES): build/examples/%: examples/%/main.c examples/%/spn.lua | $(SPN_DIR_BUILD_EXAMPLES)
	$(eval EXAMPLE := $*)
	$(eval EXAMPLE_DIR := examples/$*)
	$(eval EXAMPLE_BUILD_DIR := build/examples/$*)
	$(eval EXAMPLE_BINARY := $(EXAMPLE_BUILD_DIR)/main)

	$(call print_heading)
	@printf "building $(ANSI_FG_BRIGHT_CYAN)$(EXAMPLE_DIR)$(ANSI_RESET)"

	@echo
	@mkdir -p $(EXAMPLE_BUILD_DIR)
	$(call print_and_run,$(BOOTSTRAPPED_SPN) --lock -C $(EXAMPLE_DIR) build)
	$(call print_and_run,$(BOOTSTRAPPED_SPN) --lock -C $(EXAMPLE_DIR) copy $(EXAMPLE_BUILD_DIR))
	$(call print_and_run,$(CC) -g -o $(EXAMPLE_BINARY) $$($(BOOTSTRAPPED_SPN) -C $(EXAMPLE_DIR) print) -lm $(EXAMPLE_DIR)/main.c)
	@echo

###########
# PHONIES #
###########
build: $(SPN_BINARY)

examples: $(DEFAULT_TARGET) $(EXAMPLE_BINARIES)

install: $(DEFAULT_TARGET)
	@mkdir -p $(SPN_INSTALL_PREFIX)
	@cp $(SPN_BINARY) $(SPN_INSTALL_PREFIX)/spn
	@echo "Installed spn to $(SPN_INSTALL_PREFIX)/spn"

uninstall:
	@rm -f $(SPN_INSTALL_PREFIX)/spn

clangd: $(SPN_COMPILE_DB) $(SPN_CLANGD)

clean:
	@rm -rf $(SPN_DIR_BUILD)/bin
	@rm -rf $(SPN_DIR_BUILD)/examples
	@rm -f $(SPN_COMPILE_DB)
	@rm -f $(SPN_CLANGD)
