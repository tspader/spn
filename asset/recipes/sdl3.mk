# SPN_DIR_PROJECT: Where is the repository cloned?
# SPN_DIR_BUILD: Where is your working directory for this build?
# SPN_DIR_STORE_INCLUDE: Where should you put headers?
# SPN_DIR_STORE_BIN: Where should you put binaries?

SPN_SDL_HEADERS := $(SPN_DIR_STORE_INCLUDE)/SDL3
SPN_SDL_BINARY := $(SPN_DIR_STORE_BIN)/libSDL3.so

SPN_SDL_FLAG_DEFINES := -DCMAKE_BUILD_TYPE=$(CMAKE_TYPE) -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_EXAMPLES=OFF
SPN_SDL_CMAKE_FLAGS := $(SDL_FLAG_DEFINES)

.PHONY: spn-clone spn-build

$(SPN_DIR_PROJECT):
	git clone git@github.com:libsdl-org/sdl.git $(SPN_DIR_PROJECT)

$(SPN_SDL_BINARY):
	cmake -S$(SPN_DIR_PROJECT) -B$(SPN_DIR_BUILD) $(SPN_SDL_CMAKE_FLAGS)
	cmake --build $(SPN_DIR_BUILD) --parallel
	cp $(SPN_DIR_BUILD)/libSDL3.so $(SPN_DIR_STORE_BIN)

$(SPN_SDL_HEADERS):
	cp -r $(SPN_DIR_PROJECT)/include/SDL3 $(SPN_DIR_STORE_INCLUDE)


spn-clone: $(SPN_DIR_PROJECT)

spn-build: $(SPN_SDL_BINARY) $(SPN_SDL_HEADERS)
