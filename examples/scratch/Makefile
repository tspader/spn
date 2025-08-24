EX_BUILD := ../../build/examples
  EX_EXECUTABLE := $(EX_BUILD)/scratch

EX_FLAG_LANGUAGE := -std=c11
EX_FLAG_OPTIMIZATION := -g
EX_FLAG_OUTPUT := -o $(EX_EXECUTABLE)
#EX_FLAG_SPN := $(shell spn flags include)
EX_FLAG_SPN := -I /home/spader/.cache/spn/repo/sp
EX_CC_FLAGS := $(EX_FLAG_LANGUAGE) $(EX_FLAG_OPTIMIZATION) $(EX_FLAG_SPN) $(EX_FLAG_OUTPUT)

EX_SOURCE_FILES := main.c

all: $(EX_EXECUTABLE)

$(EX_BUILD):
	@mkdir -p $(EX_BUILD)

$(EX_EXECUTABLE): $(EX_SOURCE_FILES) | $(EX_BUILD)
	gcc $(EX_CC_FLAGS) $(EX_SOURCE_FILES)

.PHONY: all build clean
