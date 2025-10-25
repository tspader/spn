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
else ifeq ($(TARGET_OS),linux)
  CC := gcc
  CXX := g++
  SPN := spn
  FLAG_SYSTEM_LIBS := -lm -lpthread
  #FLAG_LINKAGE := -static -Wl,--gc-sections -Wl,--strip-all
  FLAG_LINKAGE := -static -g
  FLAG_RPATH := -Wl,-rpath,\$$ORIGIN
else ifeq ($(TARGET_OS),darwin)
  MACOSX_DEPLOYMENT_TARGET := 15.0
  CC := clang
  CXX := clang++
  SPN := spn
  FLAG_SYSTEM_LIBS := -framework CoreFoundation -framework Foundation -framework Cocoa -framework IOKit -framework GameController -framework ForceFeedback -framework AVFoundation -framework CoreAudio -framework AudioToolbox -framework Metal -framework MetalKit -framework Quartz -framework CoreHaptics -framework CoreMedia -framework Carbon -framework UniformTypeIdentifiers
  FLAG_LINKAGE := -g
  FLAG_RPATH := -Wl,-rpath,@loader_path
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
    BOOTSTRAP_BIN = $(SPN_DIR_BUILD_BOOTSTRAP)/bin
      BOOTSTRAP_SPN = $(BOOTSTRAP_BIN)/$(SPN)
  SPN_DIR_BUILD_EXAMPLES := $(SPN_DIR_BUILD)/examples
  SPN_DIR_BUILD_OUTPUT := $(SPN_DIR_BUILD)/bin
    SPN_OUTPUT := $(SPN_DIR_BUILD_OUTPUT)/$(SPN)
SPN_DIR_SOURCE := source
SPN_DIR_ASSET := asset
SPN_MAKEFILE := Makefile
SPN_INSTALL_PREFIX ?= $(HOME)/.local/bin



#########
# FLAGS #
#########
FLAG_LANGUAGE := -std=c11
CFLAGS := $(FLAG_LANGUAGE) $(FLAG_LINKAGE) -I$(SPN_DIR_SOURCE) -o $(SPN_OUTPUT)

.PHONY: all
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

ifeq ($(TARGET_OS),linux)
  BOOTSTRAP_ARCHIVE := $(BOOTSTRAP_ARCHIVE_LINUX)
  BOOTSTRAP_EXTRACT := tar -xf $(BOOTSTRAP_ARCHIVE) -C $(BOOTSTRAP_BIN) --strip-components=1
  BOOTSTRAP_ARCHIVE_DIR := spn-x86_64-unknown-linux-musl
else ifeq ($(TARGET_OS),darwin)
  BOOTSTRAP_ARCHIVE := $(BOOTSTRAP_ARCHIVE_DARWIN)
  BOOTSTRAP_EXTRACT := tar -xf $(BOOTSTRAP_ARCHIVE) -C $(BOOTSTRAP_BIN) --strip-components=1
  BOOTSTRAP_ARCHIVE_DIR := spn-aarch64-apple-darwin
else ifeq ($(TARGET_OS),windows)
  BOOTSTRAP_ARCHIVE := $(BOOTSTRAP_ARCHIVE_WINDOWS)
  BOOTSTRAP_EXTRACT := unzip -q $(BOOTSTRAP_ARCHIVE) -d $(BOOTSTRAP_BIN)
  BOOTSTRAP_ARCHIVE_DIR := spn-x86_64-pc-windows-gnu
endif


$(BOOTSTRAP_BIN):
	mkdir -p $(BOOTSTRAP_BIN)

$(BOOTSTRAP_SPN): $(BOOTSTRAP_ARCHIVE) | $(BOOTSTRAP_BIN)
	$(call print_heading)
	@echo "extracting spn bootstrap binary"
	@$(BOOTSTRAP_EXTRACT)
	@chmod +x $(BOOTSTRAP_SPN)

$(SPN_OUTPUT): $(BOOTSTRAP_SPN) source/*.h source/*.c $(SPN_MAKEFILE) | $(SPN_DIR_BUILD_OUTPUT)
	$(call print_heading)
	@echo "building dependencies"
	#$(BOOTSTRAP_SPN) build --output noninteractive
	$(call print_heading)
	@echo "building spn"
	#$(CC) ./source/main.c $(CFLAGS) $$($(BOOTSTRAP_SPN) print --output noninteractive) $(FLAG_SYSTEM_LIBS)
	$(CC) ./source/main.c $(CFLAGS) -I./external/sp -I./external/argparse -I$(HOME)/.local/include/luajit-2.1 $(HOME)/.local/lib/libluajit-5.1.a $(FLAG_SYSTEM_LIBS)


###########
# PHONIES #
###########
.PHONY: build windows dist smoke test install uninstall clean

build: $(SPN_OUTPUT)

windows:
	@$(MAKE) TARGET=x86_64-pc-windows-gnu

dist:
	@$(MAKE) $(SPN_OUTPUT)
	@cp $(SPN_OUTPUT) ./$(SPN)

smoke: build test
	./build/bin/test --mode debug curl sqlite cjson clay

build/bin/test: build test/main.c
	$(SPN_OUTPUT) -C test --output noninteractive build
	$(CC) ./test/main.c -g $$($(SPN_OUTPUT) -C test --output noninteractive print) -o ./build/bin/test

test: build/bin/test

install: build
	@mkdir -p $(SPN_INSTALL_PREFIX)
	@cp $(SPN_DIR_BUILD_OUTPUT)/$(SPN) $(SPN_INSTALL_PREFIX)/$(SPN)
	@echo "Installed $(SPN) to $(SPN_INSTALL_PREFIX)/$(SPN)"

uninstall:
	@rm -f $(SPN_INSTALL_PREFIX)/$(SPN)

clean:
	@rm -rf $(BOOTSTRAP_BIN)
	@rm -rf $(SPN_DIR_BUILD)/bin
	@rm -rf $(SPN_DIR_BUILD)/examples

tcc:
	tcc ./source/main.c -std=c11 -static -g -Isource -o build/bin/spn $$(build/bootstrap/bin/spn print --output noninteractive) -lm -lpthread
