HEADERS := $(SPN_DIR_STORE_INCLUDE)/sqlite3.h
BINARY := $(SPN_DIR_STORE_BIN)/libsqlite3.so

.PHONY: spn-clone spn-build

$(SPN_DIR_PROJECT):
	git clone git@github.com:sqlite/sqlite.git $(SPN_DIR_PROJECT)

$(BINARY):
	$(SPN_DIR_PROJECT)/configure
	make
	cp $(SPN_DIR_BUILD)/libsqlite3* $(SPN_DIR_STORE_BIN)

$(HEADERS): $(BINARY)
	cp -r $(SPN_DIR_BUILD)/sqlite3.h $(SPN_DIR_STORE_INCLUDE)

spn-clone: $(SPN_DIR_PROJECT)

spn-build: $(BINARY) $(HEADERS)
