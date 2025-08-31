include spn.mk

SPN_URL := git@github.com:sheredom/utest.h.git
UTEST_H := $(SPN_DIR_STORE_INCLUDE)/utest.h


.PHONY: spn-clone spn-build

$(SPN_DIR_PROJECT):
	git clone git@github.com:sheredom/utest.h.git $(SPN_DIR_PROJECT)

$(UTEST_H):
	cp $(SPN_DIR_PROJECT)/utest.h $(UTEST_H)

spn-clone: $(SPN_DIR_PROJECT)

spn-build: $(UTEST_H)

