include spn.mk

SPN_URL := git@github.com:mackron/dr_libs.git


.PHONY: spn-clone spn-build

$(SPN_DIR_PROJECT):
	git clone $(SPN_URL) $(SPN_DIR_PROJECT)

spn-clone: $(SPN_DIR_PROJECT)

spn-build:
	cp $(SPN_DIR_PROJECT)/dr_*.h $(SPN_DIR_STORE_VENDOR)
	cp $(SPN_DIR_PROJECT)/dr_*.h $(SPN_DIR_STORE_INCLUDE)
	cp -r $(SPN_DIR_PROJECT)/tests $(SPN_DIR_STORE_VENDOR)
