SPN_DIR_BUILD:= ./build
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
SPN := $(SPN_BINARY)

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


.PHONY: build sdl clangd clean examples test install uninstall all
.NOTPARALLEL: examples

all: build clangd test examples

$(SPN_DIR_BUILD_OUTPUT):
	@mkdir -p $(SPN_DIR_BUILD_OUTPUT)

$(SPN_DIR_BUILD_EXAMPLES):
	@mkdir -p $(SPN_DIR_BUILD_EXAMPLES)

$(SPN_BINARY): $(SPN_DIR_SOURCE)/main.c $(SPN_DIR_SOURCE)/spn.h  | $(SPN_DIR_BUILD_OUTPUT)
	$(eval SPN_FLAGS := $(shell spn print --compiler gcc))
	$(CC) $(CC_FLAGS) $(SPN_FLAGS) $(SPN_DIR_SOURCE)/main.c

$(SPN_TEST_BINARY): $(SPN_DIR_TEST)/main.c $(SPN_DIR_SOURCE)/spn.h $(SPN_BINARY)
	$(eval SPN_FLAGS := $(shell spn print --compiler gcc))
	$(CC) $(SPN_TEST_CC_FLAGS) $(SPN_FLAGS) $(SPN_DIR_TEST)/main.c

$(SPN_COMPILE_DB): $(SPN_MAKEFILE)

$(SPN_CLANGD): $(SPN_COMPILE_DB)
	@printf "CompileFlags:\n  Add: [$(SPN_CLANGD_HEADER_ONLY_BULLSHIT)]\n" > $(SPN_CLANGD)

EXAMPLES := $(notdir $(wildcard examples/*))
EXAMPLE_DIRS := $(addprefix examples/, $(EXAMPLES))
$(EXAMPLE_DIRS): $(SPN_DIR_BUILD)/$@ | $(SPN_DIR_BUILD_EXAMPLES)
	$(eval EXAMPLE_DIR := ./$@)
	@echo "> building $@"
	$(SPN) --lock -C $@ build
	$(eval SPN_FLAGS := $(shell $(SPN) -C $@ print --compiler gcc))
	$(CC) $(EXAMPLE_DIR)/main.c -o $(SPN_DIR_BUILD)/$@ $(SPN_FLAGS) -lm
	@echo


build: $(SPN_BINARY)

test: build $(SPN_TEST_BINARY)
	@$(SPN_TEST_BINARY)


examples: build $(EXAMPLE_DIRS)

install: build
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
