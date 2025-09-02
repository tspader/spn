include spn.mk

spn-sfh-check:
	$(call spn_check_var,SPN_SINGLE_HEADER)

spn-clone:
	@git clone $(SPN_URL) $(SPN_DIR_PROJECT)

spn-build: spn-sfh-check
	@cp -f $(SPN_DIR_PROJECT)/$(SPN_SINGLE_HEADER) $(SPN_DIR_STORE_INCLUDE)
