include spn.mk

SPN_URL := git@github.com:tspader/toml.git
TOML_H := $(SPN_DIR_STORE_INCLUDE)/toml.h


.PHONY: spn-clone spn-build

$(SPN_DIR_PROJECT):
	git clone $(SPN_URL) $(SPN_DIR_PROJECT)

$(TOML_H):
	cp $(SPN_DIR_PROJECT)/toml.h $(TOML_H)

spn-clone: $(SPN_DIR_PROJECT)

spn-build: $(TOML_H)

