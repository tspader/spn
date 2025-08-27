HEADERS := $(SPN_DIR_STORE_INCLUDE)/sqlite3.h
BINARY := $(SPN_DIR_STORE_BIN)/libsqlite3*

.PHONY: spn-clone spn-build

$(SPN_DIR_PROJECT):
	git clone git@github.com:sqlite/sqlite.git $(SPN_DIR_PROJECT)

$(BINARY):
	mkdir -p build
	pushd build
	$(SPN_DIR_PROJECT)/configure
	make
	cp $(SPN_DIR_BUILD)/libSDL3.*.so $(SPN_DIR_STORE_BIN)
	popd

$(HEADERS): $(BINARY)
	cp -r $(SPN_DIR_BUILD)/sqlite3.h $(SPN_DIR_STORE_INCLUDE)

spn-clone: $(SPN_DIR_PROJECT)

spn-build: $(BINARY) $(HEADERS)
