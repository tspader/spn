include spn.mk

SPN_URL := git@github.com:sqlite/sqlite.git
SQLITE_H := $(SPN_DIR_STORE_INCLUDE)/sqlite3.h
SQLITE_SO := $(SPN_DIR_STORE_BIN)/libsqlite3.so

.PHONY: spn-clone spn-build

$(SPN_DIR_PROJECT):
	git clone $(SPN_URL) $(SPN_DIR_PROJECT)

$(SQLITE_SO):
	$(SPN_DIR_PROJECT)/configure
	make
	cp $(SPN_DIR_BUILD)/libsqlite3* $(SPN_DIR_STORE_BIN)

$(SQLITE_H): $(SQLITE_SO)
	cp -r $(SPN_DIR_BUILD)/sqlite3.h $(SPN_DIR_STORE_INCLUDE)

spn-clone: $(SPN_DIR_PROJECT)

spn-build: $(SQLITE_SO) $(SQLITE_H)
