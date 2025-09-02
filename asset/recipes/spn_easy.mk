include spn.mk

spn-clone:
	@git clone $(SPN_URL) $(SPN_DIR_PROJECT) --quiet

spn-build:
	@echo spn_easy.mk: nothing to build for $(SPN_DIR_PROJECT)
