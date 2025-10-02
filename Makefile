###########
# Makefile #
###########
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

CARGO_DIST_TARGET ?=
ifeq ($(CARGO_DIST_TARGET),)
  UNAME_S := $(shell uname -s)
  UNAME_M := $(shell uname -m)
  ifeq ($(UNAME_S),Linux)
    TARGET := $(UNAME_M)-unknown-linux-gnu
  else ifeq ($(UNAME_S),Darwin)
    TARGET := $(UNAME_M)-apple-darwin
  endif
else
  TARGET := $(CARGO_DIST_TARGET)
endif

TARGET_PARTS := $(subst -, ,$(TARGET))
TARGET_ARCH := $(word 1,$(TARGET_PARTS))
ifeq ($(words $(TARGET_PARTS)),4)
  TARGET_VENDOR := $(word 2,$(TARGET_PARTS))
  TARGET_OS := $(word 3,$(TARGET_PARTS))
else
  TARGET_VENDOR := $(word 2,$(TARGET_PARTS))
  TARGET_OS := $(word 3,$(TARGET_PARTS))
endif

ifeq ($(TARGET_OS),windows)
  MINGW_PREFIX := $(TARGET_ARCH)-w64-mingw32
  CC := $(MINGW_PREFIX)-gcc
  CXX := $(MINGW_PREFIX)-g++
  SPN := spn.exe
  FLAG_SYSTEM_LIBS := -lws2_32 -luser32 -lkernel32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -lshell32 -lsetupapi -ladvapi32 -lgdi32
  FLAG_LINKAGE := -static -Wl,--gc-sections -Wl,--strip-all
  FLAG_RPATH :=
  SDL_LIB := libSDL3.a
  LJ_LIB := libluajit.a
  LJ_ENV :=
  LJ_FLAGS := CROSS=$(MINGW_PREFIX)- TARGET_SYS=Windows HOST_CC="gcc -m64"
  CMAKE_FLAGS := -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=$(TARGET_ARCH) -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY
else ifeq ($(TARGET_OS),linux)
  CC := gcc
  CXX := g++
  SPN := spn
  FLAG_SYSTEM_LIBS := -lm -lpthread
  #FLAG_LINKAGE := -static -Wl,--gc-sections -Wl,--strip-all
  FLAG_LINKAGE := -static -g
  FLAG_RPATH := -Wl,-rpath,\$$ORIGIN
  SDL_LIB := libSDL3.a
  LJ_LIB := libluajit.a
  LJ_ENV :=
  LJ_FLAGS := HOST_CC="gcc -m64"
  CMAKE_FLAGS :=
else ifeq ($(TARGET_OS),darwin)
  MACOSX_DEPLOYMENT_TARGET := 15.0
  CC := clang
  CXX := clang++
  SPN := spn
  FLAG_SYSTEM_LIBS := -framework CoreFoundation -framework Foundation -framework Cocoa -framework IOKit -framework GameController -framework ForceFeedback -framework AVFoundation -framework CoreAudio -framework AudioToolbox -framework Metal -framework MetalKit -framework Quartz -framework CoreHaptics -framework CoreMedia -framework Carbon -framework UniformTypeIdentifiers
  FLAG_LINKAGE :=
  FLAG_RPATH := -Wl,-rpath,@loader_path
  SDL_LIB := libSDL3.a
  LJ_LIB := libluajit.a
  LJ_ENV := export MACOSX_DEPLOYMENT_TARGET=$(MACOSX_DEPLOYMENT_TARGET) &&
  LJ_FLAGS := HOST_CC="gcc -m64"
  CMAKE_FLAGS :=
endif

#########
# PATHS #
#########
SPN_DIR_BUILD:= build
  SPN_DIR_BUILD_BOOTSTRAP:= $(SPN_DIR_BUILD)/bootstrap
    SPN_BOOTSTRAP_WORK := $(SPN_DIR_BUILD_BOOTSTRAP)/work
    SPN_BOOTSTRAP_BIN := $(SPN_DIR_BUILD_BOOTSTRAP)/bin
      SPN_BOOTSTRAP_SDL_BINARY := $(SPN_BOOTSTRAP_BIN)/$(SDL_LIB)
      SPN_BOOTSTRAP_LUAJIT_BINARY := $(SPN_BOOTSTRAP_BIN)/$(LJ_LIB)
    SPN_BOOTSTRAP_SDL := $(SPN_DIR_BUILD_BOOTSTRAP)/SDL
    SPN_BOOTSTRAP_SP := $(SPN_DIR_BUILD_BOOTSTRAP)/sp
    SPN_BOOTSTRAP_ARGPARSE := $(SPN_DIR_BUILD_BOOTSTRAP)/argparse
    SPN_BOOTSTRAP_LUAJIT := $(SPN_DIR_BUILD_BOOTSTRAP)/luajit
  SPN_DIR_BUILD_EXAMPLES := $(SPN_DIR_BUILD)/examples
  SPN_DIR_BUILD_OUTPUT := $(SPN_DIR_BUILD)/bin
    SPN_OUTPUT := $(SPN_DIR_BUILD_OUTPUT)/$(SPN)
SPN_DIR_SOURCE := source
SPN_DIR_ASSET := asset
SPN_MAKEFILE := Makefile
SPN_COMPILE_DB := compile_commands.json
SPN_INSTALL_PREFIX ?= $(HOME)/.local/bin



#########
# FLAGS #
#########
FLAG_LANGUAGE := -std=c11
BOOTSTRAP_INCLUDES := -I$(SPN_BOOTSTRAP_SDL)/include -I$(SPN_BOOTSTRAP_SP) -I$(SPN_BOOTSTRAP_ARGPARSE) -I$(SPN_BOOTSTRAP_LUAJIT)/src -I$(SPN_DIR_SOURCE)
CC_FLAGS := $(FLAG_LANGUAGE) $(FLAG_LINKAGE) $(BOOTSTRAP_INCLUDES) -o $(SPN_OUTPUT)


.PHONY: all build clangd clean examples install uninstall
.NOTPARALLEL: examples $(EXAMPLE_DIRS)

all: build


#######
# SPN #
#######
$(SPN_DIR_BUILD_OUTPUT):
	@mkdir -p $(SPN_DIR_BUILD_OUTPUT)

$(SPN_DIR_BUILD_EXAMPLES):
	@mkdir -p $(SPN_DIR_BUILD_EXAMPLES)


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
	@$(LJ_ENV) make -C $(SPN_BOOTSTRAP_LUAJIT) clean
	@$(LJ_ENV) make -C $(SPN_BOOTSTRAP_LUAJIT) amalg BUILDMODE=static $(LJ_FLAGS)
	@cp $(SPN_BOOTSTRAP_LUAJIT)/src/$(LJ_LIB) $@

$(SPN_BOOTSTRAP_SDL_BINARY): $(SPN_BOOTSTRAP_LUAJIT_BINARY) $(SPN_BOOTSTRAP_SDL) $(SPN_BOOTSTRAP_WORK) $(SPN_BOOTSTRAP_BIN)
	cmake -S$(SPN_BOOTSTRAP_SDL) -B$(SPN_BOOTSTRAP_WORK) \
	  -DCMAKE_C_COMPILER=$(CC) \
	  -DCMAKE_CXX_COMPILER=$(CXX) \
	  $(CMAKE_FLAGS) \
	  -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_AUDIO=OFF -DSDL_VIDEO=OFF -DSDL_RENDER=OFF -DSDL_CAMERA=OFF -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF -DSDL_SENSOR=OFF -DSDL_POWER=OFF -DSDL_DIALOG=OFF -DSDL_GPU=OFF -DSDL_VULKAN=OFF -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF -DSDL_WAYLAND=OFF -DSDL_X11=OFF -DSDL_KMSDRM=OFF -DSDL_OFFSCREEN=OFF -DSDL_DUMMYVIDEO=OFF -DSDL_DUMMYAUDIO=OFF -DSDL_DUMMYCAMERA=OFF -DSDL_DISKAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_PULSEAUDIO=OFF -DSDL_ALSA=OFF -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_OSS=OFF -DSDL_TRAY=OFF -DSDL_UNIX_CONSOLE_BUILD=ON
	cmake --build $(SPN_BOOTSTRAP_WORK) --parallel
	@cp $(SPN_BOOTSTRAP_WORK)/$(SDL_LIB) $@

$(SPN_OUTPUT): $(SPN_BOOTSTRAP_SDL_BINARY) $(SPN_BOOTSTRAP_LUAJIT_BINARY) $(SPN_DIR_SOURCE)/spn.h $(SPN_MAKEFILE) | $(SPN_DIR_BUILD_OUTPUT) $(SPN_BOOTSTRAP_ARGPARSE) $(SPN_BOOTSTRAP_SP)
	$(call print_heading)
	@echo "bootstrapping spn ($(TARGET_ARCH)-$(TARGET_VENDOR)-$(TARGET_OS))"

	$(call print_and_run,$(CC) -std=c11 $(FLAG_LINKAGE) $(BOOTSTRAP_INCLUDES) -o $(SPN_DIR_BUILD_OUTPUT)/$(SPN) ./source/main.c $(SPN_BOOTSTRAP_LUAJIT_BINARY) $(SPN_BOOTSTRAP_SDL_BINARY) $(FLAG_SYSTEM_LIBS))
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
	$(call print_and_run,$(SPN_OUTPUT) -C ./examples/$(1) --no-interactive build)
	$(call print_and_run,$(SPN_OUTPUT) -C ./examples/$(1) copy ./build/examples/$(1))
	$(call print_and_run,$(2) ./examples/$(1)/main.* -g -o ./build/examples/$(1)/main $$($(SPN_OUTPUT) -C ./examples/$(1) print) $(FLAG_RPATH) -lm)
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
.PHONY: $(EXAMPLES) windows help

help:
	@echo "Targets:"
	@echo "  build         Build spn binary (default)"
	@echo "  examples      Build all example projects"
	@echo "  install       Install to ~/.local/bin (or SPN_INSTALL_PREFIX)"
	@echo "  clean         Remove build artifacts"
	@echo "  clangd        Generate compile_commands.json"
	@echo ""
	@echo "Cross-compilation:"
	@echo "  make TARGET=x86_64-pc-windows-gnu"
	@echo "  make windows  (shorthand for windows target)"

build: $(SPN_OUTPUT)

windows:
	@$(MAKE) TARGET=x86_64-pc-windows-gnu

dist:
	@$(MAKE) $(SPN_OUTPUT)
	@cp $(SPN_OUTPUT) ./$(SPN)

examples: build $(EXAMPLES)

smoke: build
	@for example in $(EXAMPLES); do \
		if ! echo "$(CI_SKIP_EXAMPLES)" | grep -qw "$$example"; then \
			$(MAKE) $$example; \
		fi; \
	done

install: build
	@mkdir -p $(SPN_INSTALL_PREFIX)
	@cp $(SPN_DIR_BUILD_OUTPUT)/$(SPN) $(SPN_INSTALL_PREFIX)/$(SPN)
	@echo "Installed $(SPN) to $(SPN_INSTALL_PREFIX)/$(SPN)"

uninstall:
	@rm -f $(SPN_INSTALL_PREFIX)/$(SPN)

clangd: $(SPN_COMPILE_DB)

clean:
	@rm -rf $(SPN_DIR_BUILD)/bin
	@rm -rf $(SPN_DIR_BUILD)/examples
	@rm -rf $(SPN_DIR_BUILD_BOOTSTRAP)/work
	@rm -rf $(SPN_DIR_BUILD_BOOTSTRAP)/bin
	@rm -f $(SPN_COMPILE_DB)
