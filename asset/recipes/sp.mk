include spn.mk

SPN_URL := git@github.com:tspader/sp.git
SP_H = $(SPN_DIR_STORE_INCLUDE)/sp.h


.PHONY: spn-clone spn-build

$(SPN_DIR_PROJECT):
	git clone $(SP_URL) $(SPN_DIR_PROJECT)

$(SP_H): $(SPN_DIR_PROJECT)/sp.h
	cp $(SPN_DIR_PROJECT)/sp.h $(SP_H)

spn-clone: $(SPN_DIR_PROJECT)

spn-build: $(SP_H)

