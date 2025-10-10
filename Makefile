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
  FLAG_LINKAGE := -g
  FLAG_RPATH := -Wl,-rpath,@loader_path
  SDL_LIB := libSDL3.a
  LJ_LIB := libluajit.a
  LJ_ENV := export MACOSX_DEPLOYMENT_TARGET=$(MACOSX_DEPLOYMENT_TARGET) &&
  LJ_FLAGS := HOST_CC="clang"
  CMAKE_FLAGS :=
endif

BEAR := $(shell command -v bear 2>/dev/null)
ifneq ($(BEAR),)
  BEAR_WRAP := bear --append --
  MAKE := $(BEAR_WRAP) $(MAKE)
  CC := $(BEAR_WRAP) $(CC)
  CXX := $(BEAR_WRAP) $(CXX)
endif

#########
# PATHS #
#########
SPN_DIR_BUILD:= build
  SPN_DIR_BUILD_BOOTSTRAP:= $(SPN_DIR_BUILD)/bootstrap
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
BOOTSTRAP_ARCHIVE_LINUX := build/bootstrap/spn-x86_64-unknown-linux-musl.tar.xz
BOOTSTRAP_ARCHIVE_DARWIN := build/bootstrap/spn-aarch64-apple-darwin.tar.xz
BOOTSTRAP_ARCHIVE_WINDOWS := build/bootstrap/spn-x86_64-pc-windows-gnu.zip
BOOTSTRAP_EXTRACTED := build/bootstrap/extracted

ifeq ($(TARGET_OS),linux)
  BOOTSTRAP_ARCHIVE := $(BOOTSTRAP_ARCHIVE_LINUX)
  BOOTSTRAP_EXTRACT := tar -xf $(BOOTSTRAP_ARCHIVE) -C $(BOOTSTRAP_EXTRACTED) --strip-components=1
  BOOTSTRAP_ARCHIVE_DIR := spn-x86_64-unknown-linux-musl
else ifeq ($(TARGET_OS),darwin)
  BOOTSTRAP_ARCHIVE := $(BOOTSTRAP_ARCHIVE_DARWIN)
  BOOTSTRAP_EXTRACT := tar -xf $(BOOTSTRAP_ARCHIVE) -C $(BOOTSTRAP_EXTRACTED) --strip-components=1
  BOOTSTRAP_ARCHIVE_DIR := spn-aarch64-apple-darwin
else ifeq ($(TARGET_OS),windows)
  BOOTSTRAP_ARCHIVE := $(BOOTSTRAP_ARCHIVE_WINDOWS)
  BOOTSTRAP_EXTRACT := unzip -q $(BOOTSTRAP_ARCHIVE) -d $(BOOTSTRAP_EXTRACTED)
  BOOTSTRAP_ARCHIVE_DIR := spn-x86_64-pc-windows-gnu
endif

BOOTSTRAP_SPN := $(BOOTSTRAP_EXTRACTED)/$(SPN)

$(BOOTSTRAP_EXTRACTED):
	@mkdir -p $(BOOTSTRAP_EXTRACTED)

$(BOOTSTRAP_SPN): $(BOOTSTRAP_ARCHIVE) | $(BOOTSTRAP_EXTRACTED)
	$(call print_heading)
	@echo "extracting stage0 spn"
	@$(BOOTSTRAP_EXTRACT)
	@chmod +x $(BOOTSTRAP_SPN)

$(SPN_OUTPUT): $(BOOTSTRAP_SPN) $(SPN_DIR_SOURCE)/spn.h $(SPN_MAKEFILE) | $(SPN_DIR_BUILD_OUTPUT)
	$(call print_heading)
	@echo "building spn with stage0 ($(TARGET_ARCH)-$(TARGET_VENDOR)-$(TARGET_OS))"

	$(call print_and_run,$(BOOTSTRAP_SPN) --no-interactive build)
	$(call print_and_run,$(CC) ./source/main.c $(FLAG_LANGUAGE) $(FLAG_LINKAGE) -I$(SPN_DIR_SOURCE) -o $(SPN_OUTPUT) $$($(BOOTSTRAP_SPN) --no-interactive print) $(FLAG_SYSTEM_LIBS))
	@echo

	@printf "$(ANSI_FG_GREEN)OK!$(ANSI_RESET) try $(ANSI_FG_BRIGHT_YELLOW)make examples$(ANSI_RESET) to build some projects with your spn binary"
	@echo

############
# EXAMPLES #
############
CI_SKIP_EXAMPLES := ggml whisper ggml_static whisper_static glfw glfw_static raylib raylib_static # Just because these need an X server or GPU or whatever

EXAMPLE_DIRS := $(wildcard examples/*)
EXAMPLES_C := $(foreach dir,$(EXAMPLE_DIRS),$(if $(wildcard $(dir)/main.c),$(notdir $(dir))))
EXAMPLES_CPP := $(foreach dir,$(EXAMPLE_DIRS),$(if $(wildcard $(dir)/main.cpp),$(notdir $(dir))))
EXAMPLES := $(EXAMPLES_C) $(EXAMPLES_CPP)

EXAMPLE_TARGETS_C := $(addsuffix /main,$(addprefix build/examples/, $(EXAMPLES_C)))
EXAMPLE_TARGETS_CPP := $(addsuffix /main,$(addprefix build/examples/, $(EXAMPLES_CPP)))

EXAMPLE_MATRICES ?= debug

example_matrix_binary = $(if $(filter debug,$(1)),main,main-$(1))
example_matrix_cflags = $(if $(filter release,$(1)),-O2 -DNDEBUG,-g)

define build_example_matrix
	@printf "$(ANSI_FG_BRIGHT_BLACK)  matrix: $(3)$(ANSI_RESET)"
	@echo
	$(call print_and_run,$(SPN_OUTPUT) -C ./examples/$(1) --no-interactive --matrix $(3) clean)
	$(call print_and_run,$(SPN_OUTPUT) -C ./examples/$(1) --no-interactive --matrix $(3) build)
	@rm -rf ./build/examples/$(1)/$(3)
	@mkdir -p ./build/examples/$(1)/$(3)
	$(call print_and_run,$(SPN_OUTPUT) -C ./examples/$(1) --no-interactive --matrix $(3) copy ./build/examples/$(1)/$(3))
	$(call print_and_run,$(2) ./examples/$(1)/main.* $(call example_matrix_cflags,$(3)) -o ./build/examples/$(1)/$(call example_matrix_binary,$(3)) $$($(SPN_OUTPUT) -C ./examples/$(1) --no-interactive --matrix $(3) print) $(FLAG_RPATH) -lm)
	@:
endef

define build_example
	$(call print_heading)
	@printf "example: $(ANSI_FG_BRIGHT_CYAN)$(1)$(ANSI_RESET)"
	@echo

	@rm -rf ./build/examples/$(1)
	@mkdir -p ./build/examples/$(1)
	$(foreach matrix,$(EXAMPLE_MATRICES),$(call build_example_matrix,$(1),$(2),$(matrix)))
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
.PHONY: $(EXAMPLES) windows help test

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
	@rm -rf ./build/examples
	@for example in $(EXAMPLES); do \
		if ! echo "$(CI_SKIP_EXAMPLES)" | grep -qw "$$example"; then \
			$(MAKE) EXAMPLE_MATRICES="debug release" $$example; \
		fi; \
	done

build/bin/test: build test/main.c
	$(SPN_OUTPUT) -C test --no-interactive build
	$(CC) ./test/main.c -g $$($(SPN_OUTPUT) -C test --no-interactive print) -o ./build/bin/test

test: build/bin/test

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
