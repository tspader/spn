#########
# UTILS #
#########
ANSI_FG_CYAN := \033[36m
ANSI_FG_GREEN := \033[32m
ANSI_FG_BRIGHT_BLACK := \033[90m
ANSI_FG_BRIGHT_CYAN := \033[96m
ANSI_FG_BRIGHT_YELLOW := \033[93m
ANSI_RESET := \033[0m

define print_heading
	@printf "$(ANSI_FG_BRIGHT_CYAN)> $(ANSI_RESET)"
endef

define print_and_run
	@printf "$(ANSI_FG_BRIGHT_CYAN)>> $(ANSI_RESET)"
	@printf "$(ANSI_FG_BRIGHT_YELLOW)$1$(ANSI_RESET)"
	@echo
	@$1
endef


############
# PLATFORM #
############
ifeq ($(OS),Windows_NT)
  CC := gcc
  CXX := g++
  MAKE := make
  SDL := SDL3.lib
  LUAJIT := lua51.lib
  SPN_EXE := spn.exe
  RPATH_FLAG :=
  IS_SPN_PREINSTALLED := $(shell where $(SPN_EXE) 2>NUL)
else
  CC := gcc
  CXX := g++
  MAKE := make
  SDL := libSDL3.a
  LUAJIT := libluajit.a
  SPN_EXE := spn
  IS_SPN_PREINSTALLED := $(shell which $(SPN_EXE) 2>/dev/null)

  HAS_CLANG := $(shell which clang 2>/dev/null)
  ifdef HAS_CLANG
    CC := clang
    CXX := clang++
  endif

  HAS_BEAR := $(shell which bear 2>/dev/null)
  ifdef HAS_BEAR
    CC := bear --append -- $(CC)
    CXX := bear --append -- $(CXX)
    MAKE := bear --append -- $(MAKE)
  endif

  ifeq ($(shell uname),Darwin)
    MACOSX_DEPLOYMENT_TARGET := 15.0
    RPATH_FLAG := -Wl,-rpath,@loader_path
    SYSTEM_LIBS := -framework CoreFoundation -framework Foundation -framework Cocoa -framework IOKit -framework GameController -framework ForceFeedback -framework AVFoundation -framework CoreAudio -framework AudioToolbox -framework Metal -framework MetalKit -framework Quartz -framework CoreHaptics -framework CoreMedia -framework Carbon -framework UniformTypeIdentifiers
  endif

  ifeq ($(shell uname),Linux)
    RPATH_FLAG := -Wl,-rpath,\$$ORIGIN
    SYSTEM_LIBS += -lm -lpthread -ldl
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
SPN_INSTALL_PREFIX ?= $(HOME)/.local/bin


#########
# FLAGS #
#########
FLAG_LANGUAGE := -std=c11
FLAG_INCLUDES :=  -I$(SPN_DIR_SOURCE)
FLAG_OUTPUT := -o $(SPN_BINARY)
FLAG_OPTIMIZATION := -g -rdynamic
CC_FLAGS := $(FLAG_LANGUAGE) $(FLAG_OPTIMIZATION) $(FLAG_INCLUDES) $(FLAG_OUTPUT)


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
.NOTPARALLEL: examples $(EXAMPLE_DIRS)

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
	$(CC) ./source/main.c $(CC_FLAGS) $$($(BOOTSTRAPPED_SPN) print --compiler gcc) $(SYSTEM_LIBS)

$(SPN_COMPILE_DB): $(SPN_MAKEFILE)


#############
# BOOTSTRAP #
#############
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
	@export MACOSX_DEPLOYMENT_TARGET=$(MACOSX_DEPLOYMENT_TARGET) && make -C $(SPN_BOOTSTRAP_LUAJIT) amalg
	@cp $(SPN_BOOTSTRAP_LUAJIT)/src/$(LUAJIT) $(SPN_BOOTSTRAP_LUAJIT_BINARY)

$(SPN_BOOTSTRAP_SDL_BINARY): | $(SPN_BOOTSTRAP_SDL) $(SPN_BOOTSTRAP_WORK) $(SPN_BOOTSTRAP_BIN)
	cmake -S$(SPN_BOOTSTRAP_SDL) -B$(SPN_BOOTSTRAP_WORK) -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_AUDIO=OFF -DSDL_VIDEO=OFF -DSDL_RENDER=OFF -DSDL_CAMERA=OFF -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF -DSDL_SENSOR=OFF -DSDL_POWER=OFF -DSDL_DIALOG=OFF -DSDL_GPU=OFF -DSDL_VULKAN=OFF -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF -DSDL_WAYLAND=OFF -DSDL_X11=OFF -DSDL_KMSDRM=OFF -DSDL_OFFSCREEN=OFF -DSDL_DUMMYVIDEO=OFF -DSDL_DUMMYAUDIO=OFF -DSDL_DUMMYCAMERA=OFF -DSDL_DISKAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_PULSEAUDIO=OFF -DSDL_ALSA=OFF -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_OSS=OFF -DSDL_TRAY=OFF -DSDL_UNIX_CONSOLE_BUILD=ON
	cmake --build $(SPN_BOOTSTRAP_WORK) --parallel
	@cp $(SPN_BOOTSTRAP_WORK)/$(SDL) $(SPN_BOOTSTRAP_SDL_BINARY) 2>/dev/null

SPN_BOOTSTRAP_DEPS := $(SPN_BOOTSTRAP_SP) $(SPN_BOOTSTRAP_ARGPARSE) $(SPN_DIR_BUILD_OUTPUT)
BOOTSTRAP_INCLUDE := -I$(SPN_BOOTSTRAP_SDL)/include -I$(SPN_BOOTSTRAP_SP) -I$(SPN_BOOTSTRAP_TOML) -I$(SPN_BOOTSTRAP_ARGPARSE) -I$(SPN_BOOTSTRAP_LUAJIT)/src

bootstrap: $(SPN_BOOTSTRAP_SDL_BINARY) $(SPN_BOOTSTRAP_LUAJIT_BINARY) | $(SPN_BOOTSTRAP_DEPS)
	$(call print_heading)
	@echo "bootstrapping spn"

	$(call print_and_run,$(CC) $(CC_FLAGS) $(BOOTSTRAP_INCLUDE) ./source/main.c $(SPN_BOOTSTRAP_LUAJIT_BINARY) $(SPN_BOOTSTRAP_SDL_BINARY) $(SYSTEM_LIBS))
	@echo

	@printf "$(ANSI_FG_GREEN)OK!$(ANSI_RESET) try $(ANSI_FG_BRIGHT_YELLOW)make examples$(ANSI_RESET) to build some projects with your spn binary"
	@echo


############
# EXAMPLES #
############
CI_SKIP_EXAMPLES := ggml whisper glfw raylib # Just because these need an X server or GPU or whatever

EXAMPLE_DIRS := $(wildcard examples/*)
EXAMPLES_C := $(foreach dir,$(EXAMPLE_DIRS),$(if $(wildcard $(dir)/main.c),$(notdir $(dir))))
EXAMPLES_CPP := $(foreach dir,$(EXAMPLE_DIRS),$(if $(wildcard $(dir)/main.cpp),$(notdir $(dir))))
EXAMPLES := $(EXAMPLES_C) $(EXAMPLES_CPP)

EXAMPLE_TARGETS_C := $(addsuffix /main,$(addprefix build/examples/, $(EXAMPLES_C)))
EXAMPLE_TARGETS_CPP := $(addsuffix /main,$(addprefix build/examples/, $(EXAMPLES_CPP)))

define build_example
  $(call print_heading)
	@printf "example: $(ANSI_FG_BRIGHT_CYAN)$(1)$(ANSI_RESET)"
	@echo

	@mkdir -p ./build/examples/$(1)
	$(call print_and_run,$(BOOTSTRAPPED_SPN) -C ./examples/$(1) --no-interactive build)
	$(call print_and_run,$(BOOTSTRAPPED_SPN) -C ./examples/$(1) copy ./build/examples/$(1))
	$(call print_and_run,$(2) ./examples/$(1)/main.* -g -o ./build/examples/$(1)/main $$($(BOOTSTRAPPED_SPN) -C ./examples/$(1) print) $(RPATH_FLAG) -lm)
	@echo
endef

$(EXAMPLES): %: build/examples/%/main

$(EXAMPLE_TARGETS_C): build/examples/%/main: examples/%/main.c examples/%/spn.lua $(SPN_MAKEFILE)
	$(eval EXAMPLE := $*)
	$(call build_example,$(EXAMPLE),$(CC))

$(EXAMPLE_TARGETS_CPP): build/examples/%/main: examples/%/main.cpp examples/%/spn.lua $(SPN_MAKEFILE)
	$(eval EXAMPLE := $*)
	$(call build_example,$(EXAMPLE),$(CXX))


###########
# PHONIES #
###########
.PHONY: $(EXAMPLES)

build: $(SPN_BINARY)

dist: $(DEFAULT_TARGET)
	@cp $(SPN_BINARY) . # dist expects our binary to be in the root

examples: $(DEFAULT_TARGET) $(EXAMPLES)

smoke: $(DEFAULT_TARGET)
	@for example in $(EXAMPLES); do \
		if ! echo "$(CI_SKIP_EXAMPLES)" | grep -qw "$$example"; then \
			$(MAKE) $$example; \
		fi; \
	done

install: $(DEFAULT_TARGET)
	@mkdir -p $(SPN_INSTALL_PREFIX)
	@cp $(SPN_BINARY) $(SPN_INSTALL_PREFIX)/spn
	@echo "Installed spn to $(SPN_INSTALL_PREFIX)/spn"

uninstall:
	@rm -f $(SPN_INSTALL_PREFIX)/spn

clangd: $(SPN_COMPILE_DB)

clean:
	@rm -rf $(SPN_DIR_BUILD)/bin
	@rm -rf $(SPN_DIR_BUILD)/examples
	@rm -f $(SPN_COMPILE_DB)

