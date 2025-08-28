HEADER = $(SPN_DIR_STORE_INCLUDE)/argparse.h

.PHONY: spn-clone spn-build

$(SPN_DIR_PROJECT):
	git clone git@github.com:spaderthomas/argparse.git $(SPN_DIR_PROJECT)

$(HEADER): $(SPN_DIR_PROJECT)
	cp $(SPN_DIR_PROJECT)/argparse.h $(HEADER)

spn-clone: $(SPN_DIR_PROJECT)

spn-build: $(HEADER)

