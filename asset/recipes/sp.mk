SP_H = $(SPN_DIR_INSTALL_INCLUDE)/sp.h

.PHONY: spn-clone spn-build

$(SPN_DIR_PROJECT):
	git clone git@github.com:spaderthomas/sp.git $(SPN_DIR_PROJECT)

$(SP_H):
	cp $(SPN_DIR_PROJECT)/sp.h $(SP_H)

spn-clone: $(SPN_DIR_PROJECT)

spn-build: $(SP_H)

